#include <QCoreApplication>
#include <QDebug>

#include "keymap.h"

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        qCritical() << message;
        ++failures;
    }
}

QString validScript()
{
    return QString::fromUtf8(R"JSON({
        "switchKey": "Key_QuoteLeft",
        "mouseMoveMap": {
            "startPos": {"x": 0.5, "y": 0.42},
            "speedRatioX": 3.25,
            "speedRatioY": 1.25
        },
        "keyMapNodes": [
            {
                "type": "KMT_STEER_WHEEL",
                "centerPos": {"x": 0.2, "y": 0.7},
                "leftOffset": 0.08,
                "rightOffset": 0.08,
                "upOffset": 0.1,
                "downOffset": 0.1,
                "leftKey": "Key_A",
                "rightKey": "Key_D",
                "upKey": "Key_W",
                "downKey": "Key_S"
            },
            {
                "type": "KMT_CLICK",
                "key": "Key_F",
                "pos": {"x": 0.7, "y": 0.4},
                "switchMap": false
            }
        ]
    })JSON");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KeyMap map;
    QString error;

    check(map.loadKeyMap(validScript(), &error), "valid keymap should load");
    check(error.isEmpty(), "valid keymap should not report an error");
    check(map.isValidMouseMoveMap(), "mouse move map should be available");
    check(map.isValidSteerWheelMap(), "steer wheel map should be available");
    check(map.getKeyMapNodeKey(Qt::Key_W).type == KeyMap::KMT_STEER_WHEEL, "W should resolve to the steer wheel");
    check(map.getKeyMapNodeKey(Qt::Key_F).type == KeyMap::KMT_CLICK, "F should resolve to a click");

    QString zeroY = validScript();
    zeroY.replace("\"speedRatioY\": 1.25", "\"speedRatioY\": 0");
    error.clear();
    check(!map.loadKeyMap(zeroY, &error), "zero Y speed ratio should be rejected");
    check(!error.isEmpty(), "invalid speed ratio should report an error");
    check(!map.isValidMouseMoveMap(), "failed loading must clear partial mouse state");
    check(map.getKeyMapNodeKey(Qt::Key_W).type == KeyMap::KMT_INVALID, "failed loading must clear reverse mappings");

    QString outOfRange = validScript();
    outOfRange.replace("\"x\": 0.5, \"y\": 0.42", "\"x\": 1.2, \"y\": 0.42");
    check(!map.loadKeyMap(outOfRange, &error), "out-of-range normalized positions should be rejected");

    check(map.loadKeyMap(validScript(), &error), "a valid script should load after a failure");
    check(map.isValidMouseMoveMap(), "reload should rebuild mouse state");

    if (failures == 0) {
        qInfo() << "KeyMap tests passed";
    }
    return failures == 0 ? 0 : 1;
}
