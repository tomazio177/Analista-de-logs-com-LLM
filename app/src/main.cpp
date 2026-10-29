#include "janela.h"
#include "motor.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Analista de logs"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser linha;
    linha.addHelpOption();
    linha.addVersionOption();
    // Para desenvolvimento: abre a janela, guarda uma imagem dela e fecha. Serve para ver a app sem a abrir (com QT_QPA_PLATFORM=offscreen).
    QCommandLineOption captura(QStringLiteral("captura"), QStringLiteral("Guarda uma imagem da janela neste ficheiro e fecha."), QStringLiteral("ficheiro"));
    linha.addOption(captura);
    linha.process(app);

    Motor motor;
    JanelaPrincipal janela(&motor);
    janela.show();
    janela.carregarUltima();

    if (linha.isSet(captura)) {
        janela.resize(780, 860);
        const auto guardar = [&] {
            janela.grab().save(linha.value(captura));
            QApplication::quit();
        };
        QObject::connect(&janela, &JanelaPrincipal::carregou, &app, [&] { QTimer::singleShot(200, &app, guardar); });
        QTimer::singleShot(20000, &app, guardar); // se o motor não responder, guarda o que houver
    }
    return app.exec();
}
