#pragma once

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QListView>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QVariantAnimation>
#include <QVector>
#include <functional>

class QLabel;
class QSequentialAnimationGroup;

// A evidência: o que seguiu para o Claude (os `dados` da triagem), uma linha por aparelho, alerta, grupo de logins
// ou nota do dono. É o material que o Claude lê e que cada achado cita.

struct Campo
{
    QString texto;
    int largura = 0; // em caracteres; 0 vai até ao fim da linha
    bool forte = false;
};

struct LinhaDeEvidencia
{
    QString etiqueta; // "R" rede, "D1" aparelho, "X" desaparecido, "A1" alerta, "L" logins, "N" nota do dono
    QVector<Campo> campos;
    int gravidade = 0; // 0 nenhuma, 1 aviso, 2 erro (a mínima das regras, ou a do achado que a cita)
    // O que as citações procuram: o id ("d1", "a1"), IPs, horas, portas e processos que a linha contém.
    QString id;
    QSet<QString> ips;
    QSet<QString> horas; // "14:45" e "14:45:03"
    QSet<int> portas;
    QSet<QString> processos;
    QString dia;            // "2026-10-22", quando a linha cobre um intervalo dentro de um só dia
    int deSegundos = -1;    // o intervalo, em segundos desde a meia-noite
    int ateSegundos = -1;
    QString texto() const; // a linha inteira, para leitores de ecrã e para copiar
};

// Transforma os `dados` da triagem em linhas, por esta ordem: rede, aparelhos (e as notas de cada um), desaparecidos,
// alertas das regras (e notas), grupos de logins, notas gerais. Todo o texto passa por limparTexto.
QVector<LinhaDeEvidencia> linhasDosDados(const QJsonObject &dados);
// Um exemplo inventado, para o primeiro arranque (com IPs reservados para documentação).
QVector<LinhaDeEvidencia> linhasDeExemplo();

// As linhas (índices de 0) que um achado cita. Calculado na app, sem pedir nada ao modelo: os `alertas_relacionados`
// apontam para as linhas dos alertas, e os ids, IPs e horas escritos em `nos_dados` são procurados nas linhas.
// Portas e processos só contam numa frase sem nenhum dos outros (sozinhos apanhariam linhas a mais).
QVector<int> citacoesDoAchado(const QJsonObject &achado, const QVector<LinhaDeEvidencia> &linhas);
// Linhas seguidas juntam-se num intervalo: {14, 15, 16, 20} → {14–16, 20}.
QVector<QPair<int, int>> intervalos(QVector<int> linhas);
// "L014" ou "L015–L018" (números de 1 em diante, com tantos dígitos quantos a última linha tem, no mínimo 3).
QString nomeDoIntervalo(QPair<int, int> intervalo, int totalDeLinhas);

// --- Os widgets -------------------------------------------------------------------------------------------

class ModeloEvidencia : public QAbstractListModel
{
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;

    void definir(QVector<LinhaDeEvidencia> linhas, bool exemplo);
    const QVector<LinhaDeEvidencia> &linhas() const { return linhas_; }
    const LinhaDeEvidencia &linha(int i) const { return linhas_.at(i); }
    bool exemplo() const { return exemplo_; }

    // Marcas: linhas citadas pelo Claude (com a intensidade com que já acenderam) e o halo de uma citação clicada.
    void definirCitadas(const QVector<int> &citadas);
    bool citada(int i) const { return i < citadas_.size() && citadas_.at(i) >= 0; }
    // A ordem de uma linha citada (0, 1, 2…), para a cascata; -1 se não for citada.
    int ordemDaCitacao(int i) const { return i < citadas_.size() ? citadas_.at(i) : -1; }
    void definirGravidade(int i, int gravidade);
    qreal revelacao(int i) const; // 0..1: quanto já acendeu a marca cyan desta linha
    void definirTempoDaCascata(qreal ms) { cascata_ = ms; }
    qreal halo(int i) const { return haloLinhas_.contains(i) ? halo_ : 0; }
    void definirHalo(const QSet<int> &linhas, qreal intensidade);
    // O rato por cima e a linha atual acendem e apagam devagar: duas linhas de cada (a nova e a que sai).
    struct Transicao { int nova = -1; int antiga = -1; qreal t = 1; qreal de(int i) const { return (i == nova ? t : 0) + (i == antiga ? 1 - t : 0); } };
    Transicao rato, atual;

    int rowCount(const QModelIndex &pai = {}) const override { return pai.isValid() ? 0 : int(linhas_.size()); }
    QVariant data(const QModelIndex &i, int papel) const override;

private:
    QVector<LinhaDeEvidencia> linhas_;
    QVector<int> citadas_;
    bool exemplo_ = false;
    qreal cascata_ = 1e9; // ms desde que a cascata começou; muito grande = tudo aceso
    QSet<int> haloLinhas_;
    qreal halo_ = 0;
};

class DelegadoEvidencia : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit DelegadoEvidencia(ModeloEvidencia *modelo, QObject *pai = nullptr);
    void paint(QPainter *p, const QStyleOptionViewItem &opcao, const QModelIndex &i) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;

    static constexpr int Margem = 52; // o gutter: número da linha e as duas marcas

private:
    ModeloEvidencia *modelo_;
    QFont letra_;
    qreal larguraDoCaracter_;
};

// A barra de deslocamento é também uma minimapa: marcas de gravidade à esquerda, de citação (cyan) à direita.
// Clicar fora da pega salta para esse ponto.
class Minimapa : public QScrollBar
{
    Q_OBJECT

public:
    explicit Minimapa(ModeloEvidencia *modelo, QWidget *pai = nullptr);
    void recalcular(); // depois de mudarem as marcas

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    QRectF pega() const;
    ModeloEvidencia *modelo_;
    QVector<QPair<qreal, int>> gravidades_; // posição (0..1) e gravidade
    QVector<QPair<qreal, int>> citacoes_;   // posição (0..1) e linha
};

// O feixe de leitura: uma linha cyan com um halo que varre a área visível enquanto o Claude lê.
// Vive por cima do viewport, não recebe o rato, e cada passo repinta só a faixa antiga e a nova.
class Feixe : public QWidget
{
    Q_OBJECT

public:
    explicit Feixe(QWidget *viewport);
    void comecar();
    // A última passagem até ao fundo e o apagar. `depois` corre no fim (logo, com Reduzir movimento).
    void terminar(const std::function<void()> &depois);
    bool aVarrer() const { return ativo_; }

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void irPara(qreal y);
    QRect faixa(qreal y) const;

    static constexpr int Halo = 28;
    qreal y_ = -100;
    qreal opacidade_ = 1;
    bool ativo_ = false;
    QSequentialAnimationGroup *ciclo_;
    QVariantAnimation *passagem_;
    QVariantAnimation fim_;
};

class PainelEvidencia : public QWidget
{
    Q_OBJECT

public:
    explicit PainelEvidencia(QWidget *pai = nullptr);

    void definirDados(const QJsonObject &dados, const QString &quando);
    void mostrarExemplo();
    void limpar();
    const QVector<LinhaDeEvidencia> &linhas() const { return modelo_->linhas(); }
    int total() const { return int(modelo_->linhas().size()); }
    bool exemplo() const { return modelo_->exemplo(); }

    // Depois da triagem: as linhas citadas acendem em cascata, e as gravidades dos achados passam para as linhas.
    void marcar(const QVector<int> &citadas, const QHash<int, int> &gravidades);
    // Um chip clicado: desliza até à primeira linha, centrada, e acende o intervalo.
    void acender(int primeira, int ultima);

    void comecarLeitura();
    void terminarLeitura(const std::function<void()> &depois);
    bool aLer() const { return feixe_->aVarrer(); }

    QListView *lista() const { return lista_; }

private:
    void atualizarCabecalho();
    void mudar(ModeloEvidencia::Transicao &tr, QVariantAnimation &anim, int linha);
    bool eventFilter(QObject *o, QEvent *e) override;

    ModeloEvidencia *modelo_;
    QListView *lista_;
    Minimapa *minimapa_;
    Feixe *feixe_;
    QLabel *cabecalho_;
    QLabel *contexto_;
    QString quando_;
    QVariantAnimation cascata_;
    QVariantAnimation deslizar_;
    QVariantAnimation halo_;
    QVariantAnimation ratoAnim_;
    QVariantAnimation atualAnim_;
};
