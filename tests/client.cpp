

#include <QApplication>
#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QWidget>
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget background;
    if (argc > 3 && QString::fromLocal8Bit(argv[3]) == QLatin1String("bg")) {
        background.setStyleSheet(QStringLiteral("background: #e8e8e8;"));
        background.setWindowTitle(QStringLiteral("backdrop"));
        background.showMaximized();
    }
    QLabel label;
    label.setPixmap(QPixmap(QString::fromLocal8Bit(argv[1])));
    label.setFixedSize(label.pixmap().size());
    label.setWindowTitle(QStringLiteral("genie-test"));
    QTimer::singleShot(argc > 3 ? 300 : 0, &label, [&label]() {
        label.show();
        label.activateWindow();
    });
    QTimer::singleShot(argc > 2 ? atoi(argv[2]) : 1500, &label, [&label]() {
        label.showMinimized();
    });
    return app.exec();
}
