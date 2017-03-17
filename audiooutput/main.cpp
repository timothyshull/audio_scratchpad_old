#include <QApplication>

#include "audiooutput.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Audio Output Test");

    AudioTest audio;
    audio.show();

    return app.exec();
}