#include "janela.h"
#include "motor.h"
#include "texto.h"

#include <QDir>
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
                                 QJsonObject{{"n", 2}, {"pergunta", "Mais alguém usa este PC?"}, {"respondida", true}}}}};
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
        QVERIFY(contem(&janela, "✓ Respondida"));
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
        QVERIFY(contem(&janela, "Falta a chave da API.\nNão há ficheiro .env"));
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
