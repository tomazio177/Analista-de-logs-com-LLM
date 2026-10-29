#include "motor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <memory>

namespace {

QString procurarNode()
{
    QString node = qEnvironmentVariable("ANALISTA_NODE");
    if (!node.isEmpty())
        return QFileInfo::exists(node) ? node : QString();

    node = QStandardPaths::findExecutable(QStringLiteral("node"));
#ifdef Q_OS_WIN
    // Uma app aberta pelo Explorador nem sempre herda o PATH que o instalador do Node alterou.
    if (node.isEmpty()) {
        for (const char *variavel : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
            const QString candidato = qEnvironmentVariable(variavel) + QStringLiteral("/nodejs/node.exe");
            if (QFileInfo::exists(candidato))
                return candidato;
        }
    }
#endif
    return node;
}

// A pasta que tem src/vigiar.js: a indicada, ou a primeira acima do executável (durante o desenvolvimento a app
// está em app/build/…) ou numa pasta "motor" ao lado dele (como ficará depois de instalada).
QString procurarPasta()
{
    const QString indicada = qEnvironmentVariable("ANALISTA_MOTOR");
    if (!indicada.isEmpty())
        return QFileInfo::exists(QDir(indicada).filePath(QStringLiteral("src/vigiar.js"))) ? QDir(indicada).absolutePath() : QString();

    QDir d(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        for (const char *sub : {"", "motor/"}) {
            if (QFileInfo::exists(d.filePath(QString::fromLatin1(sub) + QStringLiteral("src/vigiar.js"))))
                return QDir(d.filePath(QString::fromLatin1(sub))).absolutePath();
        }
        if (!d.cdUp())
            break;
    }
    return {};
}

// O que cada processo vai acumulando até terminar.
struct Estado
{
    QByteArray saida;
    QByteArray andamentoPorLer;
    QByteArray erros;  // o stderr que não era andamento: serve para explicar uma falha
    bool acabou = false;
};

} // namespace

Motor::Motor(QObject *pai) : QObject(pai), localizacao_(localizar()) {}

Motor::Localizacao Motor::localizar()
{
    Localizacao l;
    l.node = procurarNode();
    l.pasta = procurarPasta();
    if (l.node.isEmpty())
        l.erro = QStringLiteral("Não encontrei o Node.js. Instala-o (nodejs.org) ou indica o caminho em ANALISTA_NODE.");
    else if (l.pasta.isEmpty())
        l.erro = QStringLiteral("Não encontrei o motor (a pasta com src/vigiar.js). Indica-a em ANALISTA_MOTOR.");
    return l;
}

void Motor::executar(const QString &script, const QStringList &args)
{
    if (!localizacao_.ok()) {
        // Sempre de forma assíncrona, como nos outros casos: quem pede ainda não ligou os sinais.
        QMetaObject::invokeMethod(this, [this, script] { emit falhou(script, localizacao_.erro); }, Qt::QueuedConnection);
        return;
    }

    auto *processo = new QProcess(this);
    auto estado = std::make_shared<Estado>();
    processo->setProgram(localizacao_.node);
    processo->setWorkingDirectory(localizacao_.pasta);
    processo->setArguments(QStringList{QStringLiteral("--disable-warning=ExperimentalWarning"),
                                       QDir(localizacao_.pasta).filePath(QStringLiteral("src/") + script)}
                           + args + QStringList{QStringLiteral("--json")});
#ifdef Q_OS_WIN
    // Sem isto, cada comando abriria durante um instante uma janela preta de consola.
    processo->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; /* CREATE_NO_WINDOW */ });
#endif

    // O stderr traz uma linha JSON por passo; o que não for isso guarda-se para explicar uma falha.
    connect(processo, &QProcess::readyReadStandardError, this, [this, processo, estado, script] {
        estado->andamentoPorLer += processo->readAllStandardError();
        int fim;
        while ((fim = estado->andamentoPorLer.indexOf('\n')) >= 0) {
            const QByteArray linha = estado->andamentoPorLer.left(fim).trimmed();
            estado->andamentoPorLer.remove(0, fim + 1);
            const QJsonObject objeto = QJsonDocument::fromJson(linha).object();
            if (objeto.contains(QStringLiteral("progresso")))
                emit progresso(script, objeto.value(QStringLiteral("progresso")).toObject());
            else if (!linha.isEmpty())
                estado->erros += linha + '\n';
        }
    });
    connect(processo, &QProcess::readyReadStandardOutput, this, [processo, estado] { estado->saida += processo->readAllStandardOutput(); });

    connect(processo, &QProcess::errorOccurred, this, [this, processo, estado, script](QProcess::ProcessError erro) {
        if (erro != QProcess::FailedToStart || estado->acabou)
            return;
        estado->acabou = true;
        // No Windows este erro sai de dentro de start(), antes de quem pediu ter ligado os sinais: emite-se sempre mais tarde.
        QMetaObject::invokeMethod(this, [this, script] { emit falhou(script, QStringLiteral("Não consegui arrancar o Node (%1).").arg(localizacao_.node)); },
                                  Qt::QueuedConnection);
        processo->deleteLater();
    });

    connect(processo, &QProcess::finished, this, [this, processo, estado, script](int codigo, QProcess::ExitStatus) {
        if (estado->acabou)
            return;
        estado->acabou = true;
        estado->saida += processo->readAllStandardOutput();
        estado->erros += estado->andamentoPorLer + processo->readAllStandardError();
        processo->deleteLater();

        QJsonParseError erroDeLeitura;
        const QJsonDocument documento = QJsonDocument::fromJson(estado->saida.trimmed(), &erroDeLeitura);
        if (erroDeLeitura.error != QJsonParseError::NoError || !documento.isObject()) {
            const QString detalhe = QString::fromUtf8(estado->erros.right(400)).trimmed();
            emit falhou(script, QStringLiteral("O motor não devolveu um resultado legível (código %1).%2")
                                    .arg(codigo)
                                    .arg(detalhe.isEmpty() ? QString() : QStringLiteral(" ") + detalhe));
            return;
        }
        const QJsonObject resultado = documento.object();
        if (resultado.value(QStringLiteral("contrato")).toInt() != ContratoSuportado) {
            emit falhou(script, QStringLiteral("A app e o motor não combinam: a app entende o contrato %1 e o motor usa o %2. Atualiza um deles.")
                                    .arg(ContratoSuportado)
                                    .arg(resultado.value(QStringLiteral("contrato")).toInt()));
            return;
        }
        emit terminou(script, resultado);
    });

    processo->start();
}
