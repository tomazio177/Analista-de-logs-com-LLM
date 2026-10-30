#pragma once

#include <QJsonObject>
#include <QMainWindow>
#include <functional>

class Botao;
class IndicadorClaude;
class LinhaDeProgresso;
class Motor;
class PainelEvidencia;
class QParallelAnimationGroup;
class QLabel;
class QScrollArea;
class QVBoxLayout;

// A janela da app: à esquerda a evidência que segue para o Claude, à direita o que ele concluiu (nível geral, achados
// com as linhas que citam, perguntas com caixa de resposta). Não calcula nada da análise: pede ao motor e mostra.
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

    // Públicos para os testes (e para a captura de ecrã) poderem alimentar a janela com resultados conhecidos.
    void mostrarTriagem(const QJsonObject &triagem);
    void mostrarVigia(const QJsonObject &vigia);
    void aoProgredir(const QString &script, const QJsonObject &evento);
    // Sem triagem nenhuma: um exemplo esbatido na evidência e o caminho até à primeira verificação.
    void primeiroArranque();
    PainelEvidencia *evidencia() const { return evidencia_; }

signals:
    // Acabou de pedir a última triagem (com ou sem sucesso).
    void carregou();

private:
    enum class Faixa { Info, Aviso, Erro };

    void aoTerminar(const QString &script, const QJsonObject &resultado);
    void aoFalhar(const QString &script, const QString &mensagem);

    void aoTerminarComoAdministrador(const QString &script, int codigo);
    void correrVigia(bool semLogs);

    void responder(int numero, const QString &texto);
    void pedirAprovacao(const QJsonObject &router);
    void pedirChave(bool primeiraVez);
    void abrirDefinicoes();

    void limparFaixas();
    void faixa(const QString &texto, Faixa tipo, const QString &botao = {}, const std::function<void()> &acao = {});
    void erroDaTriagem(const QJsonObject &triagem);
    void estadoVazio(const QString &texto);
    void definirOcupado(bool ocupado);
    void definirEstado(const QString &texto);
    void revelar();

    QWidget *cartaoDoAchado(const QJsonObject &achado, const QVector<int> &citadas);
    QWidget *cartaoDaPergunta(const QJsonObject &pergunta);

    Motor *motor_;
    // Barra
    IndicadorClaude *indicador_;
    QLabel *contexto_;
    QLabel *estado_;
    Botao *botao_;
    LinhaDeProgresso *progresso_;
    QVBoxLayout *faixas_;
    // Os dois painéis
    PainelEvidencia *evidencia_;
    QScrollArea *rolo_;
    QLabel *nivel_;
    QLabel *numeral_;
    QLabel *legenda_;
    QLabel *chamada_; // a frase grande quando não há número: primeiro arranque, a ler
    QLabel *meta_;
    QWidget *resumoCaixa_;
    QLabel *resumo_;
    QWidget *passos_;
    QVBoxLayout *avisos_;
    QVBoxLayout *achados_;
    QLabel *tituloPerguntas_;
    QVBoxLayout *perguntas_;

    bool ocupado_ = false;
    bool aprovar_ = false;
    QString pastaDeDados_;
    QList<QWidget *> porRevelar_;
    QParallelAnimationGroup *revelacao_ = nullptr;
};
