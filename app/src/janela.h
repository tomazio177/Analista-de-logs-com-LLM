#pragma once

#include <QJsonObject>
#include <QMainWindow>
#include <functional>

class Motor;
class QLabel;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

// A janela da app: o resumo da última triagem, os achados, as perguntas com caixa de resposta e o botão "Verificar agora".
// Não calcula nada: pede ao motor e mostra o que ele devolve.
class JanelaPrincipal : public QMainWindow
{
    Q_OBJECT

public:
    explicit JanelaPrincipal(Motor *motor, QWidget *pai = nullptr);

    // O que a app faz ao abrir: mostra a última triagem e, se faltar a chave da API, pede-a.
    void arrancar();
    // A última triagem guardada, sem gastar a API.
    void carregarUltima();
    // Varre a rede, lê os logins e pede a triagem (npm run vigiar). `aprovar` só depois de o dono confirmar a rede.
    void verificarAgora(bool aprovar = false);

    // Públicos para os testes poderem alimentar a janela com resultados conhecidos.
    void mostrarTriagem(const QJsonObject &triagem);
    void mostrarVigia(const QJsonObject &vigia);

signals:
    // Acabou de pedir a última triagem (com ou sem sucesso).
    void carregou();

private:
    enum class Faixa { Info, Aviso, Erro };

    void aoTerminar(const QString &script, const QJsonObject &resultado);
    void aoFalhar(const QString &script, const QString &mensagem);
    void aoProgredir(const QString &script, const QJsonObject &evento);

    void aoTerminarComoAdministrador(const QString &script, int codigo);
    void correrVigia(bool semLogs);

    void responder(int numero, const QString &texto);
    void pedirAprovacao(const QJsonObject &router);
    void pedirChave(bool primeiraVez);

    void limparFaixas();
    void faixa(const QString &texto, Faixa tipo, const QString &botao = {}, const std::function<void()> &acao = {});
    void estadoVazio(const QString &texto);
    void definirOcupado(bool ocupado);
    void definirEstado(const QString &texto);

    QWidget *cartaoDoAchado(const QJsonObject &achado);
    QWidget *cartaoDaPergunta(const QJsonObject &pergunta);

    Motor *motor_;
    QLabel *nivel_;
    QLabel *subtitulo_;
    QLabel *estado_;
    QLabel *resumo_;
    QPushButton *botao_;
    QProgressBar *progresso_;
    QVBoxLayout *faixas_;
    QVBoxLayout *avisos_;
    QVBoxLayout *achados_;
    QLabel *tituloPerguntas_;
    QVBoxLayout *perguntas_;
    bool ocupado_ = false;
    bool aprovar_ = false;
    QString pastaDeDados_;
};
