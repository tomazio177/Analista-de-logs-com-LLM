#include "administrador.h"
#include "chave.h"
#include "evidencia.h"
#include "janela.h"
#include "motor.h"
#include "texto.h"

#include <QDir>
#include <QElapsedTimer>
#include <QListView>
#include <QScrollBar>
#include <QFrame>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>

namespace {

// Define uma variável de ambiente durante um teste e repõe o que lá estava.
class VariavelTemporaria
{
public:
    VariavelTemporaria(const char *nome, const QByteArray &valor) : nome_(nome), tinha_(qEnvironmentVariableIsSet(nome)), antes_(qgetenv(nome))
    {
        qputenv(nome, valor);
    }
    ~VariavelTemporaria()
    {
        if (tinha_)
            qputenv(nome_, antes_);
        else
            qunsetenv(nome_);
    }

private:
    const char *nome_;
    bool tinha_;
    QByteArray antes_;
};

QStringList textos(const QWidget *pai)
{
    QStringList r;
    for (const QLabel *l : pai->findChildren<QLabel *>())
        r << l->text();
    return r;
}

bool contem(const QWidget *pai, const QString &pedaco)
{
    for (const QString &t : textos(pai))
        if (t.contains(pedaco))
            return true;
    return false;
}

QPushButton *botao(const QWidget *pai, const QString &texto)
{
    for (QPushButton *b : pai->findChildren<QPushButton *>())
        if (b->text() == texto)
            return b;
    return nullptr;
}

// Uma triagem como o motor a devolve (docs/contrato.md), com o que interessa testar.
QJsonObject triagemDeExemplo()
{
    const auto achado = [](const QString &titulo, const QString &gravidade) {
        return QJsonObject{{"titulo", titulo},
                           {"gravidade", gravidade},
                           {"nos_dados", QJsonArray{"28 tentativas do msedge.exe"}},
                           {"conhecimento_geral", QJsonArray{"O Edge pode repetir uma credencial guardada."}},
                           {"conclusao", "Provavelmente o Edge."},
                           {"acao_sugerida", "Rever as palavras-passe guardadas."},
                           {"confianca", "media"},
                           {"factos_sem_apoio", QJsonArray{"192.168.1.50"}}};
    };
    return QJsonObject{
        {"ok", true},
        {"nivel_geral", "atencao"},
        {"resumo", "Duas coisas a confirmar."},
        {"data", "2026-10-27T18:00:00.000Z"},
        {"avisos", QJsonArray{"O modelo não fez nenhum achado sobre o alerta a2."}},
        {"achados", QJsonArray{achado("Repetidor com SSH aberto", "baixa"), achado("Rajada vinda da rede", "alta")}},
        {"perguntas", QJsonArray{QJsonObject{{"n", 1}, {"pergunta", "De quem é o LAPTOP-AB12CD34?"}, {"respondida", false}},
                                 QJsonObject{{"n", 2}, {"pergunta", "Mais alguém usa este PC?"}, {"respondida", true},
                                             {"resposta", QJsonObject{{"texto", "Não, só eu."}, {"data", "2026-10-27T18:05:00.000Z"}}}}}}};
}

// Um motor de brincar: uma pasta com src/vigiar.js (só para ser reconhecida) e src/<script>.js à medida do teste.
struct MotorFalso
{
    QTemporaryDir pasta;
    void escrever(const QString &nome, const QByteArray &codigo)
    {
        QDir().mkpath(pasta.filePath("src"));
        QFile f(pasta.filePath("src/" + nome));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(codigo);
    }
    MotorFalso() { escrever("vigiar.js", ""); }
};

} // namespace

class TestesApp : public QObject
{
    Q_OBJECT

private slots:
    // --- Texto que vem da rede ---------------------------------------------------------------------------
    void limparTexto_tiraControlosEInversoes()
    {
        QCOMPARE(limparTexto(QStringLiteral("a\x1b[2Jb")), QStringLiteral("a[2Jb"));
        // U+202E inverte o sentido: "relatorio" + RLO + "fdp.exe" pareceria "relatorioexe.pdf".
        QCOMPARE(limparTexto(QStringLiteral("relatorio") + QChar(0x202E) + QStringLiteral("fdp.exe")), QStringLiteral("relatoriofdp.exe"));
        QCOMPARE(limparTexto(QStringLiteral("a") + QChar(0x2066) + QStringLiteral("b") + QChar(0x2069) + QStringLiteral("c") + QChar(0x200E) + QStringLiteral("d")), QStringLiteral("abcd"));
    }
    void limparTexto_naoMexeNoQueEBom()
    {
        QCOMPARE(limparTexto(QStringLiteral("Tomás — telemóvel da avó\nlinha 2\tcom tab")), QStringLiteral("Tomás — telemóvel da avó\nlinha 2\tcom tab"));
        QCOMPARE(limparTexto(QStringLiteral("iPhone 😀 de Ana")), QStringLiteral("iPhone 😀 de Ana")); // fora do plano básico: dois códigos UTF-16
        QCOMPARE(limparTexto(QString()), QString());
    }

    // --- Localizar o motor -------------------------------------------------------------------------------
    void motor_localizaPelasVariaveis()
    {
        const Motor::Localizacao l = Motor::localizar();
        QVERIFY2(l.ok(), qPrintable(l.erro));
        QVERIFY(QFile::exists(QDir(l.pasta).filePath("src/vigiar.js")));
    }
    void motor_dizOQueFaltaEmVezDeFalharEmSilencio()
    {
        {
            VariavelTemporaria motor("ANALISTA_MOTOR", "/nao/existe");
            QVERIFY(Motor::localizar().erro.contains("motor"));
        }
        VariavelTemporaria node("ANALISTA_NODE", "/nao/existe/node");
        QVERIFY(Motor::localizar().erro.contains("Node"));
    }

    // --- Falar com o motor a sério -----------------------------------------------------------------------
    void motor_devolveOsErrosDoContrato()
    {
        QTemporaryDir pasta;
        VariavelTemporaria base("ANALISTA_DB", pasta.filePath("teste.db").toUtf8());
        Motor motor;
        QSignalSpy depois(&motor, &Motor::terminou);
        motor.executar("triar.js", {"--ultima"});
        QVERIFY(depois.wait(30000));
        const QJsonObject r = depois.first().at(1).toJsonObject();
        QCOMPARE(r.value("contrato").toInt(), Motor::ContratoSuportado);
        QCOMPARE(r.value("ok").toBool(), false);
        QCOMPARE(r.value("erro").toObject().value("codigo").toString(), QString("sem-triagem"));
    }
    void motor_leOAndamentoENaoMisturaComOResultado()
    {
        MotorFalso falso;
        falso.escrever("passos.js", R"(
            process.stderr.write(JSON.stringify({ progresso: { fase: 'descobrir', feitos: 1, total: 2 } }) + '\n');
            process.stderr.write('um aviso qualquer do Node\n');
            process.stderr.write(JSON.stringify({ progresso: { fase: 'descobrir', feitos: 2, total: 2 } }) + '\n');
            process.stdout.write(JSON.stringify({ contrato: 1, ok: true, argumentos: process.argv.slice(2) }) + '\n');
        )");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        QSignalSpy andamento(&motor, &Motor::progresso);
        QSignalSpy depois(&motor, &Motor::terminou);
        motor.executar("passos.js", {"--texto=-começa com traço", "e espaços"});
        QVERIFY(depois.wait(30000));

        QCOMPARE(andamento.size(), 2);
        QCOMPARE(andamento.at(1).at(1).toJsonObject().value("feitos").toInt(), 2);
        // Os argumentos chegam inteiros ao motor, com acentos, espaços e um traço à frente.
        const QJsonArray args = depois.first().at(1).toJsonObject().value("argumentos").toArray();
        QCOMPARE(args.at(0).toString(), QString("--texto=-começa com traço"));
        QCOMPARE(args.at(1).toString(), QString("e espaços"));
        QCOMPARE(args.last().toString(), QString("--json"));
    }
    void motor_naoMostraLixoQuandoOMotorRebenta()
    {
        MotorFalso falso;
        falso.escrever("rebenta.js", "process.stderr.write('Erro: ficheiro em falta\\n'); process.exit(3);");
        falso.escrever("lixo.js", "process.stdout.write('isto não é JSON');");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        QSignalSpy falhou(&motor, &Motor::falhou);
        motor.executar("rebenta.js");
        motor.executar("lixo.js");
        QTRY_COMPARE_WITH_TIMEOUT(falhou.size(), 2, 30000);
        const QString mensagem = falhou.at(0).at(0).toString() == "rebenta.js" ? falhou.at(0).at(1).toString() : falhou.at(1).at(1).toString();
        QVERIFY2(mensagem.contains("código 3") && mensagem.contains("ficheiro em falta"), qPrintable(mensagem));
    }
    void motor_recusaUmContratoQueNaoEntende()
    {
        MotorFalso falso;
        falso.escrever("futuro.js", "process.stdout.write(JSON.stringify({ contrato: 2, ok: true }));");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        QSignalSpy falhou(&motor, &Motor::falhou);
        motor.executar("futuro.js");
        QVERIFY(falhou.wait(30000));
        QVERIFY(falhou.first().at(1).toString().contains("não combinam"));
    }
    void motor_dizSeONodeNaoArranca()
    {
        QTemporaryFile naoExecutavel; // existe, mas não é um programa
        QVERIFY(naoExecutavel.open());
        naoExecutavel.write("nada");
        naoExecutavel.flush();
        VariavelTemporaria node("ANALISTA_NODE", naoExecutavel.fileName().toUtf8());
        Motor motor;
        QSignalSpy falhou(&motor, &Motor::falhou);
        motor.executar("triar.js", {"--ultima"});
        // Não se usa wait(): se o sinal já tivesse saído de dentro do executar(), o wait() esperaria por outro que nunca vem.
        QTRY_COMPARE_WITH_TIMEOUT(falhou.size(), 1, 30000);
        QVERIFY(falhou.first().at(1).toString().contains("arrancar o Node"));
    }

    // --- A janela ----------------------------------------------------------------------------------------
    void janela_mostraAchadosPorGravidadeEPerguntasPorResponder()
    {
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarTriagem(triagemDeExemplo());

        const QStringList t = textos(&janela);
        QVERIFY(t.contains("ATENÇÃO"));
        QVERIFY(contem(&janela, "Duas coisas a confirmar."));
        QVERIFY(contem(&janela, "⚠ O modelo não fez nenhum achado sobre o alerta a2."));
        QVERIFY(contem(&janela, "Citado como facto, mas não está nos dados enviados: 192.168.1.50"));
        // O achado de gravidade alta vem antes do de gravidade baixa, mesmo tendo chegado depois.
        QVERIFY(t.indexOf("Rajada vinda da rede") < t.indexOf("Repetidor com SSH aberto"));
        QVERIFY(t.indexOf("Rajada vinda da rede") >= 0);
        // Só a pergunta por responder tem caixa de resposta.
        QCOMPARE(janela.findChildren<QLineEdit *>().size(), 1);
        QVERIFY(contem(&janela, "✓ Já respondeste: «Não, só eu.»"));
    }
    void janela_mostraOTextoDaRedeComoTextoENaoComoHtml()
    {
        QJsonObject t = triagemDeExemplo();
        QJsonObject achado = t.value("achados").toArray().first().toObject();
        achado["titulo"] = QStringLiteral("<img src='x'><b>Telemóvel</b>") + QChar(0x202E) + QStringLiteral("gnp");
        t["achados"] = QJsonArray{achado};
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarTriagem(t);

        QLabel *titulo = nullptr;
        for (QLabel *l : janela.findChildren<QLabel *>())
            if (l->text().contains("Telemóvel"))
                titulo = l;
        QVERIFY(titulo);
        QCOMPARE(titulo->textFormat(), Qt::PlainText);
        QCOMPARE(titulo->text(), QString("<img src='x'><b>Telemóvel</b>gnp")); // literal, sem a inversão de direção
    }
    void janela_aTriagemSeguinteSubstituiAAnterior()
    {
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarTriagem(triagemDeExemplo());
        QJsonObject nova = triagemDeExemplo();
        nova["nivel_geral"] = "tranquilo";
        nova["achados"] = QJsonArray{};
        nova["perguntas"] = QJsonArray{};
        nova["avisos"] = QJsonArray{};
        janela.mostrarTriagem(nova);

        QVERIFY(textos(&janela).contains("TRANQUILO"));
        QVERIFY(!contem(&janela, "Rajada vinda da rede"));
        QVERIFY(!contem(&janela, "De quem é o LAPTOP"));
        QCOMPARE(janela.findChildren<QLineEdit *>().size(), 0);
    }
    void janela_explicaOQueCorreuMalNaVigia()
    {
        const QJsonObject vigia{
            {"ok", false},
            {"varrimento", QJsonObject{{"ok", false}, {"erro", QJsonObject{{"codigo", "rede-por-aprovar"}, {"mensagem", "Rede nova"}}},
                                       {"router", QJsonObject{{"ip", "192.168.0.1"}, {"mac", "00:00:5e:00:53:02"}}}}},
            {"logins", QJsonObject{{"ok", false}, {"erro", QJsonObject{{"codigo", "sem-permissao"}, {"mensagem", "x"}}}}},
            {"triagem", QJsonObject{{"ok", false}, {"erro", QJsonObject{{"codigo", "sem-chave"}, {"mensagem", "Falta a chave da API."}}}, {"detalhe", "Não há ficheiro .env"}}},
        };
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarVigia(vigia);

        QVERIFY(botao(&janela, "Aprovar e verificar…"));
        QVERIFY(contem(&janela, "ainda não foi aprovada"));
        QVERIFY(contem(&janela, "precisam de administrador"));
        QVERIFY(contem(&janela, "Falta a chave da API para pedir a triagem."));
        QVERIFY(botao(&janela, "Guardar a chave…"));
    }
    void janela_foraDoWindowsNaoSeQueixaDosLogins()
    {
        const QJsonObject vigia{
            {"ok", true},
            {"varrimento", QJsonObject{{"ok", true}, {"aparelhos", QJsonArray{1, 2, 3}}, {"mudancas", QJsonArray{1}}}},
            {"logins", QJsonObject{{"ok", false}, {"erro", QJsonObject{{"codigo", "nao-windows"}, {"mensagem", "só no Windows"}}}}},
            {"triagem", triagemDeExemplo()},
        };
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarVigia(vigia);
        QVERIFY(!contem(&janela, "só no Windows"));
        QVERIFY(contem(&janela, "3 aparelhos, 1 mudanças"));
    }
    void janela_respondeAUmaPerguntaDeCabecaAoMotor()
    {
        // O motor de brincar devolve a triagem de exemplo e regista os argumentos com que a resposta chegou.
        MotorFalso falso;
        QJsonObject exemplo = triagemDeExemplo();
        exemplo["contrato"] = 1;
        falso.escrever("triar.js", "process.stdout.write(JSON.stringify(" + QJsonDocument(exemplo).toJson(QJsonDocument::Compact) + "));");
        falso.escrever("notas.js", "require('node:fs').writeFileSync(__dirname + '/registo.json', JSON.stringify(process.argv.slice(2)));"
                                   "process.stdout.write(JSON.stringify({ contrato: 1, ok: true }));");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        JanelaPrincipal janela(&motor);
        QSignalSpy carregou(&janela, &JanelaPrincipal::carregou);
        janela.carregarUltima();
        QTRY_COMPARE_WITH_TIMEOUT(carregou.size(), 1, 30000);

        auto *caixa = janela.findChild<QLineEdit *>();
        QVERIFY(caixa);
        caixa->setText(QStringLiteral("  -é de alguém de casa  "));
        QTest::mouseClick(botao(&janela, "Guardar"), Qt::LeftButton);

        const QString registo = falso.pasta.filePath("src/registo.json");
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(registo), 30000);
        QFile f(registo);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonArray args = QJsonDocument::fromJson(f.readAll()).array();
        // Uma só peça "--texto=…": uma resposta que comece por traço não é tomada por outra opção.
        QCOMPARE(args.at(0).toString(), QString("--pergunta"));
        QCOMPARE(args.at(1).toString(), QString("1"));
        QCOMPARE(args.at(2).toString(), QString("--texto=-é de alguém de casa"));
        // Guardada a resposta, a janela volta a pedir a última triagem para se atualizar.
        QTRY_COMPARE_WITH_TIMEOUT(carregou.size(), 2, 30000);
    }
    void janela_logins_lidos_a_parte_nao_dao_faixa_vazia()
    {
        // Com os logins lidos como administrador, a vigia corre com --sem-logs e o motor devolve logins: null.
        const QJsonObject vigia{{"ok", true},
                                {"varrimento", QJsonObject{{"ok", true}, {"aparelhos", QJsonArray{}}, {"mudancas", QJsonArray{}}}},
                                {"logins", QJsonValue::Null},
                                {"triagem", triagemDeExemplo()}};
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarVigia(vigia);
        for (QFrame *f : janela.findChildren<QFrame *>())
            if (f->objectName() == "faixa")
                QFAIL("não devia haver faixas");
    }


    // --- A evidência e as citações -----------------------------------------------------------------------
    void evidencia_citacoesVemDosIdsIPsEHoras()
    {
        const QJsonObject dados{
            {"rede", "192.0.2.0/24"},
            {"aparelhos", QJsonArray{QJsonObject{{"id", "d1"}, {"ip", "192.0.2.1"}, {"nome", "router"}, {"portas_abertas", QJsonArray{"80 http"}}},
                                     QJsonObject{{"id", "d2"}, {"ip", "192.0.2.34"}, {"nome", "<b>portátil</b>"}, {"portas_abertas", QJsonArray{"23 telnet"}}}}},
            {"logins_falhados", QJsonObject{
                 {"alertas_das_regras", QJsonArray{QJsonObject{{"id", "a1"}, {"tipo", "rajada"}, {"origem", "este PC"}, {"n", 28}, {"de", "2026-10-29 14:45:03"}, {"ate", "2026-10-29 14:45:20"}, {"gravidade_minima", "alta"}}}},
                 {"agrupados", QJsonArray{QJsonObject{{"ip_origem", "127.0.0.1"}, {"processo", "msedge.exe"}, {"tentativas", 28}, {"primeira", "2026-10-29 14:45:03"}, {"ultima", "2026-10-29 14:45:20"}},
                                          QJsonObject{{"ip_origem", "127.0.0.1"}, {"processo", "svchost.exe"}, {"tentativas", 1}, {"primeira", "2026-10-24 09:00:00"}, {"ultima", "2026-10-24 09:00:00"}}}}}}};
        const QVector<LinhaDeEvidencia> l = linhasDosDados(dados);
        // R, D1, D2, R (logins), A1, e os logins por ordem: L 09:00 (dia 24), L 14:45 (dia 29)
        QCOMPARE(l.size(), 7);
        QCOMPARE(l[2].campos[1].texto, QString("<b>portátil</b>")); // texto da rede fica texto
        QCOMPARE(l[4].gravidade, 2);

        const auto achado = [](QJsonArray alertas, QJsonArray nosDados) { return QJsonObject{{"alertas_relacionados", alertas}, {"nos_dados", nosDados}}; };
        // O alerta pelo id; a hora dentro do intervalo de um grupo de logins também é dele.
        QCOMPARE(citacoesDoAchado(achado({"a1"}, {"pico às 14:45:10"}), l), (QVector<int>{4, 6}));
        // Um IP aponta para o aparelho; "d1" escrito na frase também conta.
        QCOMPARE(citacoesDoAchado(achado({}, {"192.0.2.34 e o d1"}), l), (QVector<int>{1, 2}));
        // A porta sozinha só conta quando a frase não tem ids, IPs nem horas.
        QCOMPARE(citacoesDoAchado(achado({}, {"a porta 23 telnet aberta"}), l), (QVector<int>{2}));
        QCOMPARE(citacoesDoAchado(achado({}, {"nada que se encontre"}), l), QVector<int>{});

        QCOMPARE(intervalos({20, 14, 16, 15, 15}), (QVector<QPair<int, int>>{{14, 16}, {20, 20}}));
        QCOMPARE(nomeDoIntervalo({13, 13}, 45), QString("L014"));
        QCOMPARE(nomeDoIntervalo({14, 17}, 1200), QString("L0015–L0018"));
    }
    void evidencia_aguenta100milLinhasSemEngasgar()
    {
        QJsonArray aparelhos;
        for (int i = 0; i < 100000; ++i)
            aparelhos.append(QJsonObject{{"id", QString("d%1").arg(i + 1)}, {"ip", QString("10.%1.%2.%3").arg(i / 65536).arg(i / 256 % 256).arg(i % 256)},
                                         {"nome", QString("aparelho-%1").arg(i)}, {"portas_abertas", QJsonArray{"80 http"}}});
        PainelEvidencia painel;
        painel.resize(640, 800);
        painel.show();
        QElapsedTimer relogio;
        relogio.start();
        painel.definirDados(QJsonObject{{"aparelhos", aparelhos}}, QString());
        QVector<int> citadas;
        for (int i = 0; i < 100000; i += 997)
            citadas << i;
        painel.marcar(citadas, {});
        QVERIFY2(relogio.elapsed() < 3000, qPrintable(QString("carregar levou %1 ms").arg(relogio.elapsed())));
        QCOMPARE(painel.total(), 100000);

        // Percorre a lista toda aos saltos e pinta cada vista: tem de caber folgado num frame de 60 fps.
        QScrollBar *barra = painel.lista()->verticalScrollBar();
        const int passos = 200;
        relogio.restart();
        for (int i = 0; i <= passos; ++i) {
            barra->setValue(barra->maximum() * i / passos);
            painel.lista()->viewport()->repaint();
        }
        const double porVista = double(relogio.elapsed()) / (passos + 1);
        QVERIFY2(porVista < 16.0, qPrintable(QString("cada vista levou %1 ms").arg(porVista)));
    }
    void janela_cadaAchadoMostraAsLinhasQueCita()
    {
        QJsonObject t = triagemDeExemplo();
        t["dados"] = QJsonObject{{"aparelhos", QJsonArray{QJsonObject{{"id", "d1"}, {"ip", "192.168.1.20"}, {"nome", "tv"}}}}};
        QJsonArray achados = t.value("achados").toArray();
        QJsonObject primeiro = achados.first().toObject();
        primeiro["nos_dados"] = QJsonArray{"a tv em 192.168.1.20"};
        achados[0] = primeiro;
        t["achados"] = achados;
        Motor motor;
        JanelaPrincipal janela(&motor);
        janela.mostrarTriagem(t);
        // O achado que cita a linha tem o chip; o outro diz que não encontrou nenhuma.
        QVERIFY(botao(&janela, "L001"));
        QVERIFY(contem(&janela, "SEM LINHA ENCONTRADA NA EVIDÊNCIA"));
        botao(&janela, "L001")->click();
        QCOMPARE(janela.evidencia()->lista()->currentIndex().row(), 0);
    }

    // --- Pedir administrador (as aspas da linha de comandos do Windows) ----------------------------------
    void linhaDeComandos_seguraOsCaminhosComEspacos()
    {
        QCOMPARE(linhaDeComandos({"--json", "--dados=C:\\Users\\Ana\\AppData"}), QString("--json --dados=C:\\Users\\Ana\\AppData"));
        QCOMPARE(linhaDeComandos({"C:\\Program Files\\Analista\\motor\\src\\logs.js"}), QString("\"C:\\Program Files\\Analista\\motor\\src\\logs.js\""));
        // Uma barra no fim, dentro de aspas, tem de ser duplicada; senão engoliria a aspa final.
        QCOMPARE(linhaDeComandos({"--dados=C:\\Pasta com espaço\\"}), QString("\"--dados=C:\\Pasta com espaço\\\\\""));
        QCOMPARE(linhaDeComandos({"diz \"olá\""}), QString("\"diz \\\"olá\\\"\""));
        QCOMPARE(linhaDeComandos({""}), QString("\"\""));
    }

    // --- A chave da API ----------------------------------------------------------------------------------
    void motor_mandaAEntradaPeloStdin()
    {
        MotorFalso falso;
        falso.escrever("eco.js", "let t='';process.stdin.on('data',d=>t+=d);process.stdin.on('end',()=>process.stdout.write(JSON.stringify({contrato:1,ok:true,recebido:t})));");
        falso.escrever("sem_stdin.js", "process.stdin.resume();process.stdin.on('end',()=>process.stdout.write(JSON.stringify({contrato:1,ok:true})));");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        QSignalSpy depois(&motor, &Motor::terminou);
        motor.executar("eco.js", {}, QStringLiteral("sk-ant-segredo").toUtf8());
        // Um comando que lê o stdin sem lhe mandarmos nada não fica à espera para sempre.
        motor.executar("sem_stdin.js");
        QTRY_COMPARE_WITH_TIMEOUT(depois.size(), 2, 30000);
        for (const auto &sinal : depois)
            if (sinal.at(0).toString() == "eco.js")
                QCOMPARE(sinal.at(1).toJsonObject().value("recebido").toString(), QString("sk-ant-segredo"));
    }
    void dialogoChave_ficaAbertoComOErroEFechaQuandoGuarda()
    {
        MotorFalso falso;
        falso.escrever("config.js", R"(
            let t = ''; process.stdin.on('data', (d) => (t += d));
            process.stdin.on('end', () => process.stdout.write(JSON.stringify(t === 'sk-ant-boa'
              ? { contrato: 1, ok: true, aviso: 'Sem internet: ficou guardada na mesma.' }
              : { contrato: 1, ok: false, erro: { codigo: 'chave-recusada', mensagem: 'A Anthropic recusou esta chave.' } })));
        )");
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", falso.pasta.path().toUtf8());
        Motor motor;
        DialogoChave dialogo(&motor, true);
        dialogo.show();
        auto *caixa = dialogo.findChild<QLineEdit *>();
        QCOMPARE(caixa->echoMode(), QLineEdit::Password);

        caixa->setText("sk-ant-errada");
        QTest::mouseClick(botao(&dialogo, "Guardar"), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(contem(&dialogo, "A Anthropic recusou esta chave."), 30000);
        QVERIFY(dialogo.isVisible());
        QVERIFY(botao(&dialogo, "Guardar")->isEnabled());

        caixa->setText("  sk-ant-boa  ");
        QTest::mouseClick(botao(&dialogo, "Guardar"), Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(dialogo.result(), int(QDialog::Accepted), 30000);
        QCOMPARE(dialogo.aviso(), QString("Sem internet: ficou guardada na mesma."));
    }
    void janela_semMotorDizPorqueNaoFunciona()
    {
        VariavelTemporaria pastaDoMotor("ANALISTA_MOTOR", "/nao/existe");
        Motor motor;
        JanelaPrincipal janela(&motor);
        QVERIFY(contem(&janela, "Não encontrei o motor"));
        QVERIFY(!botao(&janela, "Verificar agora")->isEnabled());
    }
};

QTEST_MAIN(TestesApp)
#include "testes.moc"
