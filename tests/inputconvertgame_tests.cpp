#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QVector>

#include "controller.h"

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        qCritical() << message;
        ++failures;
    }
}

QString mouseScript()
{
    return QString::fromUtf8(R"JSON({
        "switchKey": "Key_QuoteLeft",
        "mouseMoveMap": {
            "startPos": {"x": 0.5, "y": 0.5},
            "speedRatioX": 1.0,
            "speedRatioY": 1.0
        },
        "keyMapNodes": []
    })JSON");
}

QString delayedClickScript()
{
    return QString::fromUtf8(R"JSON({
        "switchKey": "Key_QuoteLeft",
        "keyMapNodes": [
            {
                "type": "KMT_CLICK_MULTI",
                "key": "Key_F",
                "clickNodes": [
                    {"delay": 50, "pos": {"x": 0.5, "y": 0.5}}
                ]
            }
        ]
    })JSON");
}

void toggleCustomMode(Controller &controller)
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::NoModifier, "`");
    controller.keyEvent(&event, QSize(1920, 1080), QSize(960, 540));
}

void sendMouseMove(Controller &controller, qreal x, qreal y)
{
    const QPointF position(x, y);
    QMouseEvent event(QEvent::MouseMove, position, position, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    controller.mouseEvent(&event, QSize(1920, 1080), QSize(960, 540));
    QCoreApplication::sendPostedEvents();
}

quint16 readBigEndian16(const QByteArray &data, int offset)
{
    return (static_cast<quint8>(data.at(offset)) << 8) | static_cast<quint8>(data.at(offset + 1));
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QVector<QByteArray> firstMessages;
    QVector<QByteArray> secondMessages;
    bool firstGrabbed = false;
    bool secondGrabbed = false;
    Controller first([&firstMessages](const QByteArray &data) {
        firstMessages.append(data);
        return static_cast<qint64>(data.size());
    }, mouseScript());
    Controller second([&secondMessages](const QByteArray &data) {
        secondMessages.append(data);
        return static_cast<qint64>(data.size());
    }, mouseScript());
    QObject::connect(&first, &Controller::grabCursor, [&firstGrabbed](bool grabbed) { firstGrabbed = grabbed; });
    QObject::connect(&second, &Controller::grabCursor, [&secondGrabbed](bool grabbed) { secondGrabbed = grabbed; });

    toggleCustomMode(first);
    toggleCustomMode(second);
    check(first.isCurrentCustomKeymap() && second.isCurrentCustomKeymap(), "both controllers should enter custom mode");
    check(firstGrabbed && secondGrabbed, "custom mode should request cursor capture");

    sendMouseMove(first, 200, 200);
    sendMouseMove(second, 200, 200);
    sendMouseMove(first, 210, 200);
    sendMouseMove(second, 210, 200);
    sendMouseMove(first, 220, 200);
    sendMouseMove(second, 220, 200);
    check(firstMessages.size() == 3, "first controller should send DOWN and two MOVE messages");
    check(secondMessages.size() == 3, "second controller must not lose a MOVE sent at the same coordinates");
    if (firstMessages.size() >= 3) {
        check(readBigEndian16(firstMessages.at(2), 22) == 0xffff, "MOVE pressure should remain pressed");
    }

    first.releaseAllTouches();
    second.releaseAllTouches();
    check(!first.isCurrentCustomKeymap() && !second.isCurrentCustomKeymap(), "release should leave custom mode");
    check(!firstGrabbed && !secondGrabbed, "release should synchronously drop cursor capture");
    check(firstMessages.size() == 4 && secondMessages.size() == 4, "release should synchronously send one UP per controller");

#ifndef Q_OS_MACOS
    QKeyEvent nativeFifty(QEvent::KeyPress, Qt::Key_2, Qt::NoModifier, 0, 50, 0, "2");
    first.keyEvent(&nativeFifty, QSize(1920, 1080), QSize(960, 540));
    check(!first.isCurrentCustomKeymap(), "native key code 50 must not toggle mapping outside macOS");
#endif

    QVector<QByteArray> delayedMessages;
    Controller delayed([&delayedMessages](const QByteArray &data) {
        delayedMessages.append(data);
        return static_cast<qint64>(data.size());
    }, delayedClickScript());
    toggleCustomMode(delayed);
    QKeyEvent click(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier, "f");
    delayed.keyEvent(&click, QSize(1920, 1080), QSize(960, 540));
    delayed.releaseAllTouches();
    QEventLoop waitLoop;
    QTimer::singleShot(120, &waitLoop, &QEventLoop::quit);
    waitLoop.exec();
    QCoreApplication::sendPostedEvents();
    check(delayedMessages.isEmpty(), "released delayed clicks must not inject later touches");

    QString invalid = mouseScript();
    invalid.replace("\"speedRatioY\": 1.0", "\"speedRatioY\": 0");
    Controller fallback([](const QByteArray &data) { return static_cast<qint64>(data.size()); }, invalid);
    toggleCustomMode(fallback);
    check(!fallback.isCurrentCustomKeymap(), "invalid scripts should fall back to normal input");

    if (failures == 0) {
        qInfo() << "Input conversion tests passed";
    }
    return failures == 0 ? 0 : 1;
}
