#include "janela.h"
#include "motor.h"
#include "tema.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Analista de logs"));
    QApplication::setApplicationName(QStringLiteral("Analista de logs"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icone/analista.png")));
    Tema::aplicar(app);

    QCommandLineParser linha;
    linha.addHelpOption();
    linha.addVersionOption();
    // Para desenvolvimento: abre a janela, guarda uma imagem dela e fecha. Serve para ver a app sem a abrir (com QT_QPA_PLATFORM=offscreen).
    QCommandLineOption captura(QStringLiteral("captura"), QStringLiteral("Guarda uma imagem da janela neste ficheiro e fecha."), QStringLiteral("ficheiro"));
    // Com --cena, a janela não fala com o motor: mostra uma triagem de exemplo (--exemplo) num dos estados.
    // Assim as capturas nunca levam dados reais da rede de casa.
    QCommandLineOption cena(QStringLiteral("cena"), QStringLiteral("triagem, escrita, lendo, citacao, vazio ou erro."), QStringLiteral("nome"));
    QCommandLineOption exemplo(QStringLiteral("exemplo"), QStringLiteral("Ficheiro JSON com uma triagem de exemplo, com dados."), QStringLiteral("ficheiro"));
    linha.addOptions({captura, cena, exemplo});
    linha.process(app);

    Motor motor;
    JanelaPrincipal janela(&motor);
    if (linha.isSet(captura))
        janela.resize(1280, 800);
    janela.show();

    const auto guardar = [&] {
        janela.grab().save(linha.value(captura));
        QApplication::quit();
    };

    if (!linha.isSet(cena)) {
        janela.arrancar();
        if (linha.isSet(captura)) {
            QObject::connect(&janela, &JanelaPrincipal::carregou, &app, [&] { QTimer::singleShot(1500, &app, guardar); });
            QTimer::singleShot(20000, &app, guardar); // se o motor não responder, guarda o que houver
        }
        return app.exec();
    }

    QJsonObject triagem;
    if (linha.isSet(exemplo)) {
        QFile f(linha.value(exemplo));
        if (f.open(QIODevice::ReadOnly))
            triagem = QJsonDocument::fromJson(f.readAll()).object();
    }
    const QString nome = linha.value(cena);
    int espera = 1400;
    if (nome == QLatin1String("vazio")) {
        janela.primeiroArranque();
    } else if (nome == QLatin1String("lendo")) {
        QTimer::singleShot(100, &janela, [&] {
            janela.aoProgredir(QStringLiteral("vigiar.js"), {{"fase", "inicio"}, {"rede", "192.0.2.0/24"}});
            janela.aoProgredir(QStringLiteral("vigiar.js"), {{"fase", "triagem"}, {"dados", triagem.value(QStringLiteral("dados"))}});
        });
        espera = 1150;
    } else if (nome == QLatin1String("erro")) {
        janela.mostrarTriagem(triagem);
        janela.mostrarVigia({{"ok", false},
                             {"varrimento", QJsonObject{{"ok", true}, {"aparelhos", QJsonArray{}}, {"mudancas", QJsonArray{}}}},
                             {"logins", QJsonValue::Null},
                             {"triagem", QJsonObject{{"ok", false}, {"erro", QJsonObject{{"codigo", "limite-de-pedidos"}, {"mensagem", "Limite de pedidos atingido."}}}}}});
    } else {
        janela.mostrarTriagem(triagem);
        if (nome == QLatin1String("escrita"))
            espera = 520; // a meio: os achados a entrar e a primeira conclusão a escrever-se
        if (nome == QLatin1String("citacao")) {
            // Carrega no primeiro chip de citação, como quem quer ver de onde vem a conclusão.
            QTimer::singleShot(1200, &janela, [&] {
                for (QPushButton *b : janela.findChildren<QPushButton *>())
                    if (b->text().startsWith(QLatin1Char('L')) && b->text().size() >= 4 && b->text().at(1).isDigit()) {
                        b->click();
                        break;
                    }
            });
            espera = 1750;
        }
    }
    if (linha.isSet(captura))
        QTimer::singleShot(espera, &app, guardar);
    return app.exec();
}
