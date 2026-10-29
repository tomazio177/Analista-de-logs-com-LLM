#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

// Fala com o motor (o programa em Node): corre um comando com --json, lê o documento que ele devolve e o andamento
// que escreve no stderr. Não sabe nada do que os comandos significam: o contrato está em docs/contrato.md.
class Motor : public QObject
{
    Q_OBJECT

public:
    // A versão do contrato que esta app entende. Se o motor devolver outra, a app avisa em vez de mostrar lixo.
    static constexpr int ContratoSuportado = 1;

    struct Localizacao
    {
        QString node;   // o executável do Node
        QString pasta;  // a pasta do projeto, onde está src/
        QString erro;   // vazio se tudo foi encontrado
        bool ok() const { return erro.isEmpty(); }
    };

    explicit Motor(QObject *pai = nullptr);

    // ANALISTA_NODE e ANALISTA_MOTOR mandam; senão procura o Node no PATH e a pasta do motor a partir do executável.
    static Localizacao localizar();

    const Localizacao &localizacao() const { return localizacao_; }

    // Corre `src/<script> <args> --json`. Pode haver vários ao mesmo tempo: cada um tem o seu processo.
    // `entrada` vai para o stdin: é por aí que segue a chave da API, que na linha de comandos ficaria à vista de outros programas.
    void executar(const QString &script, const QStringList &args = {}, const QByteArray &entrada = {});

    // Corre `src/<script> <args>` com privilégios de administrador: o Windows pergunta à pessoa. Não há resultado JSON
    // (o Windows não deixa ler o que um processo elevado escreve); só o código de saída, ou -1 se a pessoa recusou.
    void executarComoAdministrador(const QString &script, const QStringList &args = {});

signals:
    // Uma linha de andamento ({"progresso": {...}}), já sem o invólucro.
    void progresso(const QString &script, const QJsonObject &evento);
    // O documento do motor, com ok verdadeiro ou falso (e, nesse caso, erro.codigo).
    void terminou(const QString &script, const QJsonObject &resultado);
    // Não houve documento: o Node não arrancou, o motor rebentou ou devolveu lixo.
    void falhou(const QString &script, const QString &mensagem);
    // Acabou um executarComoAdministrador: o código de saída, ou -1 se a pessoa recusou ou não arrancou.
    void terminouComoAdministrador(const QString &script, int codigo);

private:
    Localizacao localizacao_;
};
