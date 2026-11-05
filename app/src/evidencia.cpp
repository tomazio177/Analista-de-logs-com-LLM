#include "evidencia.h"

#include "componentes.h"
#include "tema.h"
#include "texto.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPauseAnimation>
#include <QRegularExpression>
#include <QSequentialAnimationGroup>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using Tema::Cor;

// --- Das linhas de dados às linhas de evidência -----------------------------------------------------------

namespace {

const QRegularExpression &reIp()
{
    static const QRegularExpression r(QStringLiteral(R"(\b\d{1,3}(?:\.\d{1,3}){3}\b)"));
    return r;
}

// "2026-10-22 14:45:03" (a hora local que segue para o modelo) → dia e segundos desde a meia-noite.
bool partirDataHora(const QString &t, QString *dia, int *segundos)
{
    static const QRegularExpression r(QStringLiteral(R"(^(\d{4}-\d{2}-\d{2})[ T](\d{2}):(\d{2})(?::(\d{2}))?)"));
    const auto m = r.match(t);
    if (!m.hasMatch())
        return false;
    *dia = m.captured(1);
    *segundos = m.captured(2).toInt() * 3600 + m.captured(3).toInt() * 60 + m.captured(4).toInt();
    return true;
}

QString curta(const QString &t) // "2026-10-22 14:45:03" → "10-22 14:45:03"
{
    return t.size() >= 19 ? t.mid(5, 14) : t;
}

QString soHora(const QString &t)
{
    return t.size() >= 19 ? t.mid(11, 8) : t;
}

void juntarHoras(LinhaDeEvidencia &l, const QString &de, const QString &ate)
{
    for (const QString &t : {de, ate}) {
        if (t.size() >= 19) {
            l.horas << t.mid(11, 5) << t.mid(11, 8);
        }
    }
    QString diaDe, diaAte;
    int s1 = 0, s2 = 0;
    if (partirDataHora(de, &diaDe, &s1) && partirDataHora(ate.isEmpty() ? de : ate, &diaAte, &s2) && diaDe == diaAte) {
        l.dia = diaDe;
        l.deSegundos = s1;
        l.ateSegundos = s2;
    }
}

void juntarIp(LinhaDeEvidencia &l, const QString &texto)
{
    auto it = reIp().globalMatch(texto);
    while (it.hasNext())
        l.ips << it.next().captured(0);
}

QString texto(const QJsonValue &v)
{
    if (v.isDouble())
        return QString::number(v.toDouble());
    return limparTexto(v.toString());
}

QStringList lista(const QJsonValue &v)
{
    QStringList r;
    for (const QJsonValue &x : v.toArray())
        r << limparTexto(x.toString());
    return r;
}

int gravidadeDe(const QString &g)
{
    if (g == QLatin1String("alta"))
        return 2;
    if (g == QLatin1String("media"))
        return 1;
    return 0;
}

void notasDe(const QJsonObject &o, QVector<LinhaDeEvidencia> &linhas)
{
    for (const QJsonValue &n : o.value(QStringLiteral("notas_do_dono")).toArray()) {
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("N");
        const QJsonObject nota = n.toObject();
        l.campos = {{curta(texto(nota.value(QStringLiteral("quando")))), 16},
                    {QStringLiteral("o dono disse: ") + texto(nota.value(QStringLiteral("texto"))), 0, true}};
        linhas << l;
    }
}

} // namespace

QString LinhaDeEvidencia::texto() const
{
    QStringList partes{etiqueta};
    for (const Campo &c : campos)
        if (!c.texto.isEmpty())
            partes << c.texto;
    return partes.join(QStringLiteral("  "));
}

QVector<LinhaDeEvidencia> linhasDosDados(const QJsonObject &d)
{
    QVector<LinhaDeEvidencia> r;
    if (d.isEmpty())
        return r;

    if (d.contains(QStringLiteral("rede"))) {
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("R");
        const QJsonObject vig = d.value(QStringLiteral("vigilancia")).toObject();
        l.campos = {{texto(d.value(QStringLiteral("rede"))), 18, true},
                    {QStringLiteral("varrido ") + curta(texto(d.value(QStringLiteral("ultimo_varrimento")))), 22},
                    {QStringLiteral("%1 varrimentos desde %2").arg(vig.value(QStringLiteral("varrimentos_feitos")).toInt()).arg(texto(vig.value(QStringLiteral("desde"))).left(10)), 0}};
        r << l;
    }

    for (const QJsonValue &v : d.value(QStringLiteral("aparelhos")).toArray()) {
        const QJsonObject a = v.toObject();
        LinhaDeEvidencia l;
        l.id = a.value(QStringLiteral("id")).toString().toLower();
        l.etiqueta = l.id.toUpper();
        const QString ip = texto(a.value(QStringLiteral("ip")));
        l.ips << ip;
        QStringList portas = lista(a.value(QStringLiteral("portas_abertas")));
        for (const QString &p : portas)
            l.portas << p.section(QLatin1Char(' '), 0, 0).toInt();
        const QString nome = a.value(QStringLiteral("nome")).isString() ? texto(a.value(QStringLiteral("nome"))) : QStringLiteral("(sem nome)");
        const QString fabricante = a.value(QStringLiteral("mac_privado")).toBool() ? QStringLiteral("mac privado") : texto(a.value(QStringLiteral("fabricante_mac")));
        l.campos = {{ip, 15, true},
                    {nome, 24, true},
                    {fabricante, 12},
                    {portas.isEmpty() ? QStringLiteral("sem portas") : portas.join(QStringLiteral(", ")), 30},
                    {a.value(QStringLiteral("este_pc")).toBool() ? QStringLiteral("este PC") : QStringLiteral("desde ") + texto(a.value(QStringLiteral("visto_pela_primeira_vez"))).left(10), 0}};
        r << l;
        notasDe(a, r);
    }

    for (const QJsonValue &v : d.value(QStringLiteral("aparelhos_desaparecidos")).toArray()) {
        const QJsonObject a = v.toObject();
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("X");
        const QString ip = texto(a.value(QStringLiteral("ultimo_ip")));
        l.ips << ip;
        l.campos = {{ip, 15, true}, {QStringLiteral("desapareceu"), 24}, {QStringLiteral("%1 varrimentos sem resposta").arg(a.value(QStringLiteral("varrimentos_sem_resposta")).toInt()), 0}};
        r << l;
    }

    const QJsonObject logins = d.value(QStringLiteral("logins_falhados")).toObject();
    if (!logins.isEmpty()) {
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("R");
        const int buracos = logins.value(QStringLiteral("periodos_possivelmente_incompletos")).toArray().size();
        l.campos = {{QStringLiteral("logins falhados"), 18, true},
                    {QStringLiteral("%1 em %2 dias").arg(logins.value(QStringLiteral("total")).toInt()).arg(logins.value(QStringLiteral("periodo_dias")).toInt()), 22},
                    {buracos ? QStringLiteral("%1 períodos possivelmente incompletos").arg(buracos)
                             : QStringLiteral("lido ") + curta(texto(logins.value(QStringLiteral("ultima_leitura")))),
                     0}};
        r << l;
    }

    for (const QJsonValue &v : logins.value(QStringLiteral("alertas_das_regras")).toArray()) {
        const QJsonObject a = v.toObject();
        LinhaDeEvidencia l;
        l.id = a.value(QStringLiteral("id")).toString().toLower();
        l.etiqueta = l.id.toUpper();
        l.gravidade = gravidadeDe(a.value(QStringLiteral("gravidade_minima")).toString());
        const QString origem = texto(a.value(QStringLiteral("origem")));
        juntarIp(l, origem);
        const QString de = texto(a.value(QStringLiteral("de"))), ate = texto(a.value(QStringLiteral("ate")));
        juntarHoras(l, de, ate);
        QStringList extra = lista(a.value(QStringLiteral("processos")));
        for (const QString &p : extra)
            l.processos << p.toLower();
        if (a.contains(QStringLiteral("utilizadores")))
            extra << lista(a.value(QStringLiteral("utilizadores")));
        else if (a.contains(QStringLiteral("utilizadores_distintos")))
            extra << QStringLiteral("%1 utilizadores").arg(a.value(QStringLiteral("utilizadores_distintos")).toInt());
        l.campos = {{a.value(QStringLiteral("tipo")).toString().toUpper(), 22, true},
                    {origem, 15, true},
                    {QStringLiteral("%1×").arg(a.value(QStringLiteral("n")).toInt()), 6, true},
                    {curta(de) + QStringLiteral("–") + soHora(ate), 24},
                    {extra.join(QStringLiteral(", ")), 0}};
        r << l;
        notasDe(a, r);
    }

    QVector<QJsonObject> grupos;
    for (const QJsonValue &v : logins.value(QStringLiteral("agrupados")).toArray())
        grupos << v.toObject();
    std::stable_sort(grupos.begin(), grupos.end(), [](const QJsonObject &a, const QJsonObject &b) {
        return a.value(QStringLiteral("primeira")).toString() < b.value(QStringLiteral("primeira")).toString();
    });
    for (const QJsonObject &g : grupos) {
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("L");
        const QString primeira = texto(g.value(QStringLiteral("primeira"))), ultima = texto(g.value(QStringLiteral("ultima")));
        juntarHoras(l, primeira, ultima);
        const QString ip = texto(g.value(QStringLiteral("ip_origem")));
        juntarIp(l, ip);
        const QString processo = texto(g.value(QStringLiteral("processo")));
        if (!processo.isEmpty())
            l.processos << processo.toLower();
        const int n = g.value(QStringLiteral("tentativas")).toInt();
        const QString utilizador = texto(g.value(QStringLiteral("utilizador")));
        QStringList resto{texto(g.value(QStringLiteral("tipo"))), texto(g.value(QStringLiteral("motivo")))};
        if (!utilizador.startsWith(QLatin1Char('(')))
            resto.prepend(utilizador);
        resto.removeAll(QString());
        l.campos = {{curta(primeira) + (n > 1 && ultima != primeira ? QStringLiteral("–") + soHora(ultima) : QString()), 24, true},
                    {QStringLiteral("%1×").arg(n), 6, true},
                    {ip.isEmpty() ? QStringLiteral("—") : ip, 15},
                    {processo.isEmpty() ? QStringLiteral("—") : processo, 16},
                    {resto.join(QStringLiteral(" · ")), 0}};
        r << l;
    }

    for (const QJsonValue &n : d.value(QStringLiteral("notas_gerais")).toArray()) {
        LinhaDeEvidencia l;
        l.etiqueta = QStringLiteral("N");
        const QJsonObject nota = n.toObject();
        l.campos = {{curta(texto(nota.value(QStringLiteral("quando")))), 16}, {QStringLiteral("o dono disse: ") + texto(nota.value(QStringLiteral("texto"))), 0, true}};
        r << l;
    }
    return r;
}

QVector<LinhaDeEvidencia> linhasDeExemplo()
{
    // Uma casa inventada, com endereços reservados para documentação (RFC 5737).
    const QJsonObject dados{
        {"rede", "192.0.2.0/24"},
        {"ultimo_varrimento", "2026-10-30 21:13:40"},
        {"vigilancia", QJsonObject{{"varrimentos_feitos", 14}, {"desde", "2026-10-01 19:02:11"}}},
        {"aparelhos", QJsonArray{
             QJsonObject{{"id", "d1"}, {"ip", "192.0.2.1"}, {"nome", "router.lan"}, {"fabricante_mac", "00:00:5e"}, {"portas_abertas", QJsonArray{"53 dns", "80 http", "443 https"}}, {"visto_pela_primeira_vez", "2026-10-01 19:02:11"}},
             QJsonObject{{"id", "d2"}, {"ip", "192.0.2.20"}, {"nome", "tv-sala.lan"}, {"fabricante_mac", "00:00:5e"}, {"portas_abertas", QJsonArray{"8008 chromecast"}}, {"visto_pela_primeira_vez", "2026-10-01 19:02:11"}},
             QJsonObject{{"id", "d3"}, {"ip", "192.0.2.34"}, {"nome", "LAPTOP-AB12CD34.lan"}, {"mac_privado", true}, {"portas_abertas", QJsonArray{"445 smb"}}, {"visto_pela_primeira_vez", "2026-10-29 22:41:05"}},
             QJsonObject{{"id", "d4"}, {"ip", "192.0.2.77"}, {"nome", "PC-Sala.lan"}, {"este_pc", true}, {"fabricante_mac", "00:00:5e"}, {"portas_abertas", QJsonArray{"445 smb"}}}}},
        {"logins_falhados", QJsonObject{
             {"periodo_dias", 7}, {"total", 31}, {"ultima_leitura", "2026-10-30 21:13:52"},
             {"alertas_das_regras", QJsonArray{QJsonObject{{"id", "a1"}, {"tipo", "rajada"}, {"origem", "este PC"}, {"n", 28}, {"de", "2026-10-29 14:45:03"}, {"ate", "2026-10-29 14:45:20"}, {"processos", QJsonArray{"msedge.exe"}}, {"gravidade_minima", QJsonValue::Null}},
                                               QJsonObject{{"id", "a2"}, {"tipo", "pela-rede"}, {"origem", "192.0.2.34"}, {"n", 3}, {"de", "2026-10-29 22:43:10"}, {"ate", "2026-10-29 22:44:02"}, {"utilizadores", QJsonArray{"admin"}}, {"gravidade_minima", "media"}}}},
             {"agrupados", QJsonArray{QJsonObject{{"utilizador", "(não enviado: login neste PC)"}, {"tipo", "interativo"}, {"motivo", "palavra-passe errada"}, {"ip_origem", "127.0.0.1"}, {"processo", "msedge.exe"}, {"tentativas", 28}, {"primeira", "2026-10-29 14:45:03"}, {"ultima", "2026-10-29 14:45:20"}},
                                      QJsonObject{{"utilizador", "admin"}, {"tipo", "rede"}, {"motivo", "utilizador inexistente"}, {"ip_origem", "192.0.2.34"}, {"processo", ""}, {"tentativas", 3}, {"primeira", "2026-10-29 22:43:10"}, {"ultima", "2026-10-29 22:44:02"}}}}}},
    };
    return linhasDosDados(dados);
}

// --- Citações ---------------------------------------------------------------------------------------------

QVector<int> citacoesDoAchado(const QJsonObject &achado, const QVector<LinhaDeEvidencia> &linhas)
{
    QSet<int> r;
    QSet<QString> ids;
    for (const QJsonValue &v : achado.value(QStringLiteral("alertas_relacionados")).toArray())
        ids << v.toString().toLower();

    static const QRegularExpression reId(QStringLiteral(R"(\b([ad]\d+)\b)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reHora(QStringLiteral(R"(\b(\d{1,2}):(\d{2})(?::(\d{2}))?\b)"));
    static const QRegularExpression rePortas(QStringLiteral(R"(\bportas?\s+(\d{1,5}(?:\s*(?:,|e)\s*\d{1,5})*))"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reProcesso(QStringLiteral(R"(\b[\w.-]+\.exe\b)"), QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < linhas.size(); ++i)
        if (!linhas[i].id.isEmpty() && ids.contains(linhas[i].id))
            r << i;

    for (const QJsonValue &v : achado.value(QStringLiteral("nos_dados")).toArray()) {
        const QString frase = v.toString();
        QSet<QString> idsDaFrase, ipsDaFrase, horas;
        QVector<int> segundos;
        for (auto it = reId.globalMatch(frase); it.hasNext();)
            idsDaFrase << it.next().captured(1).toLower();
        for (auto it = reIp().globalMatch(frase); it.hasNext();)
            ipsDaFrase << it.next().captured(0);
        for (auto it = reHora.globalMatch(frase); it.hasNext();) {
            const auto m = it.next();
            const int h = m.captured(1).toInt(), mi = m.captured(2).toInt();
            if (h > 23 || mi > 59)
                continue;
            const QString hhmm = QStringLiteral("%1:%2").arg(h, 2, 10, QLatin1Char('0')).arg(mi, 2, 10, QLatin1Char('0'));
            horas << (m.captured(3).isEmpty() ? hhmm : hhmm + QLatin1Char(':') + m.captured(3));
            segundos << h * 3600 + mi * 60 + m.captured(3).toInt();
        }

        bool achou = false;
        for (int i = 0; i < linhas.size(); ++i) {
            const LinhaDeEvidencia &l = linhas[i];
            bool bate = (!l.id.isEmpty() && idsDaFrase.contains(l.id)) || l.ips.intersects(ipsDaFrase) || l.horas.intersects(horas);
            // Uma hora dentro do intervalo de um grupo de logins ou de um alerta também é dele.
            if (!bate && l.deSegundos >= 0)
                for (int s : segundos)
                    if (s >= l.deSegundos && s <= l.ateSegundos)
                        bate = true;
            if (bate) {
                r << i;
                achou = true;
            }
        }
        if (achou)
            continue;

        QSet<int> portas;
        for (auto it = rePortas.globalMatch(frase); it.hasNext();)
            for (const QString &p : it.next().captured(1).split(QRegularExpression(QStringLiteral(R"(\s*(?:,|e)\s*)"))))
                portas << p.toInt();
        QSet<QString> processos;
        for (auto it = reProcesso.globalMatch(frase); it.hasNext();)
            processos << it.next().captured(0).toLower();
        for (int i = 0; i < linhas.size(); ++i)
            if (linhas[i].portas.intersects(portas) || linhas[i].processos.intersects(processos))
                r << i;
    }
    QVector<int> ordenadas(r.begin(), r.end());
    std::sort(ordenadas.begin(), ordenadas.end());
    return ordenadas;
}

QVector<QPair<int, int>> intervalos(QVector<int> linhas)
{
    std::sort(linhas.begin(), linhas.end());
    linhas.erase(std::unique(linhas.begin(), linhas.end()), linhas.end());
    QVector<QPair<int, int>> r;
    for (int l : linhas) {
        if (!r.isEmpty() && r.last().second + 1 == l)
            r.last().second = l;
        else
            r.append({l, l});
    }
    return r;
}

QString nomeDoIntervalo(QPair<int, int> intervalo, int totalDeLinhas)
{
    const int digitos = std::max(3, int(QString::number(totalDeLinhas).size()));
    const auto n = [digitos](int i) { return QStringLiteral("L%1").arg(i + 1, digitos, 10, QLatin1Char('0')); };
    return intervalo.first == intervalo.second ? n(intervalo.first) : n(intervalo.first) + QStringLiteral("–") + n(intervalo.second);
}

// --- ModeloEvidencia --------------------------------------------------------------------------------------

void ModeloEvidencia::definir(QVector<LinhaDeEvidencia> linhas, bool exemplo)
{
    beginResetModel();
    linhas_ = std::move(linhas);
    exemplo_ = exemplo;
    citadas_.fill(-1, linhas_.size());
    haloLinhas_.clear();
    halo_ = 0;
    endResetModel();
}

void ModeloEvidencia::definirCitadas(const QVector<int> &citadas)
{
    citadas_.fill(-1, linhas_.size());
    int ordem = 0;
    for (int i : citadas)
        if (i >= 0 && i < citadas_.size() && citadas_[i] < 0)
            citadas_[i] = ordem++;
}

void ModeloEvidencia::definirGravidade(int i, int gravidade)
{
    if (i >= 0 && i < linhas_.size())
        linhas_[i].gravidade = std::max(linhas_[i].gravidade, gravidade);
}

qreal ModeloEvidencia::revelacao(int i) const
{
    const int ordem = ordemDaCitacao(i);
    if (ordem < 0)
        return 0;
    return qBound(0.0, (cascata_ - ordem * 40.0) / 180.0, 1.0);
}

void ModeloEvidencia::definirHalo(const QSet<int> &linhas, qreal intensidade)
{
    haloLinhas_ = linhas;
    halo_ = intensidade;
}

QVariant ModeloEvidencia::data(const QModelIndex &i, int papel) const
{
    if (!i.isValid() || i.row() >= linhas_.size())
        return {};
    if (papel == Qt::DisplayRole || papel == Qt::AccessibleTextRole || papel == Qt::ToolTipRole)
        return linhas_[i.row()].texto();
    return {};
}

// --- DelegadoEvidencia ------------------------------------------------------------------------------------

DelegadoEvidencia::DelegadoEvidencia(ModeloEvidencia *modelo, QObject *pai)
    : QStyledItemDelegate(pai), modelo_(modelo), letra_(Tema::fonte(Tema::Letra::Log))
{
    larguraDoCaracter_ = QFontMetricsF(letra_).horizontalAdvance(QLatin1Char('0'));
}

QSize DelegadoEvidencia::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {200, Tema::AlturaDaLinha};
}

void DelegadoEvidencia::paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &indice) const
{
    const int i = indice.row();
    const LinhaDeEvidencia &l = modelo_->linha(i);
    const QRect r = o.rect;
    p->save();
    if (modelo_->exemplo())
        p->setOpacity(0.4);

    // O fundo: linha selecionada ou com o rato, e o halo de uma citação clicada.
    if (const qreal s = modelo_->atual.de(i); s > 0) {
        QColor c = Tema::cor(Cor::SuperficieElevada);
        c.setAlphaF(s);
        p->fillRect(r, c);
    } else if (const qreal h = modelo_->rato.de(i); h > 0) {
        QColor c = Tema::cor(Cor::Superficie);
        c.setAlphaF(h);
        p->fillRect(r, c);
    }
    if (const qreal h = modelo_->halo(i); h > 0) {
        QColor c = Tema::cor(Cor::ClaudeTenue);
        c.setAlphaF(c.alphaF() * h * 1.6);
        p->fillRect(r, c);
    }

    p->setFont(letra_);
    const QFontMetricsF fm(letra_);
    const qreal base = r.top() + (r.height() + fm.ascent() - fm.descent()) / 2.0;

    // O gutter: número da linha, a régua de gravidade e a marca do Claude.
    p->setPen(Tema::cor(Cor::TextoTenue));
    const QString numero = QString::number(i + 1).rightJustified(3, QLatin1Char('0'));
    p->drawText(QPointF(36 - fm.horizontalAdvance(numero), base), numero);
    if (l.gravidade > 0)
        p->fillRect(QRectF(40, r.top() + 2, 2, r.height() - 4), Tema::cor(l.gravidade == 2 ? Cor::Erro : Cor::Aviso));
    if (const qreal v = modelo_->revelacao(i); v > 0) {
        QColor c = Tema::cor(Cor::Claude);
        c.setAlphaF(v);
        p->fillRect(QRectF(44, r.top() + 1, 2, r.height() - 2), c);
    }

    // A etiqueta e os campos, em colunas fixas.
    qreal x = r.left() + Margem;
    p->setPen(Tema::cor(Cor::TextoSecundario));
    p->drawText(QPointF(x, base), l.etiqueta);
    x += larguraDoCaracter_ * 5;
    for (const Campo &c : l.campos) {
        const qreal largura = c.largura > 0 ? c.largura * larguraDoCaracter_ : r.right() - x - 8;
        if (largura <= 0)
            break;
        p->setPen(Tema::cor(c.forte ? Cor::Texto : Cor::TextoSecundario));
        p->drawText(QPointF(x, base), fm.elidedText(c.texto, Qt::ElideRight, largura - larguraDoCaracter_));
        x += largura + larguraDoCaracter_;
        if (x > r.right())
            break;
    }
    p->restore();
}

// --- Minimapa ---------------------------------------------------------------------------------------------

Minimapa::Minimapa(ModeloEvidencia *modelo, QWidget *pai) : QScrollBar(Qt::Vertical, pai), modelo_(modelo)
{
    setFixedWidth(12);
    setCursor(Qt::PointingHandCursor);
}

void Minimapa::recalcular()
{
    gravidades_.clear();
    citacoes_.clear();
    const auto &linhas = modelo_->linhas();
    const qreal n = std::max<qsizetype>(1, linhas.size());
    for (int i = 0; i < linhas.size(); ++i) {
        if (linhas[i].gravidade > 0)
            gravidades_.append({i / n, linhas[i].gravidade});
        if (modelo_->citada(i))
            citacoes_.append({i / n, i});
    }
    update();
}

QRectF Minimapa::pega() const
{
    const qreal total = maximum() - minimum() + pageStep();
    if (total <= 0 || maximum() <= minimum())
        return {};
    const qreal h = std::max(24.0, height() * pageStep() / total);
    const qreal y = (height() - h) * (value() - minimum()) / qreal(maximum() - minimum());
    return {0, y, qreal(width()), h};
}

void Minimapa::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Tema::cor(Cor::Substrato));
    p.fillRect(QRectF(0, 0, 1, height()), Tema::cor(Cor::Hairline));
    const qreal alto = height();
    // Marcas de 2 px; várias no mesmo pixel ficam numa só. A gravidade à esquerda, o Claude à direita.
    for (const auto &[pos, g] : gravidades_)
        p.fillRect(QRectF(2, std::floor(pos * (alto - 2)), 4, 2), Tema::cor(g == 2 ? Cor::Erro : Cor::Aviso));
    for (const auto &[pos, linha] : citacoes_) {
        QColor c = Tema::cor(Cor::Claude);
        c.setAlphaF(modelo_->revelacao(linha));
        p.fillRect(QRectF(7, std::floor(pos * (alto - 2)), 4, 2), c);
    }
    const QRectF h = pega();
    if (!h.isEmpty()) {
        QColor fundo = Tema::cor(Cor::Texto);
        fundo.setAlphaF(isSliderDown() || underMouse() ? 0.10 : 0.05);
        p.fillRect(h.adjusted(1, 0, 0, 0), fundo);
        p.setPen(Tema::cor(Cor::HairlineForte));
        p.drawRect(h.adjusted(1.5, 0.5, -0.5, -0.5));
    }
}

void Minimapa::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !pega().contains(e->position()) && maximum() > minimum()) {
        // Salta para o ponto clicado, com a pega centrada nele.
        const qreal frac = qBound(0.0, e->position().y() / height(), 1.0);
        const int total = maximum() - minimum() + pageStep();
        setValue(minimum() + int(frac * total) - pageStep() / 2);
        e->accept();
        return;
    }
    QScrollBar::mousePressEvent(e);
}

// --- Feixe ------------------------------------------------------------------------------------------------

Feixe::Feixe(QWidget *viewport) : QWidget(viewport), ciclo_(new QSequentialAnimationGroup(this)), passagem_(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(viewport->rect());
    viewport->installEventFilter(this);
    hide();

    passagem_->setStartValue(0.0);
    passagem_->setEndValue(1.0);
    passagem_->setDuration(1800);
    passagem_->setEasingCurve(Tema::ciclo());
    connect(passagem_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) { irPara(v.toReal() * height()); });
    ciclo_->addAnimation(passagem_);
    ciclo_->addPause(300);
    ciclo_->setLoopCount(-1);

    fim_.setEasingCurve(Tema::chegada());
    connect(&fim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        // Primeiro desce até ao fundo; no último terço apaga-se.
        const qreal t = v.toReal();
        opacidade_ = t < 0.66 ? 1.0 : 1.0 - (t - 0.66) / 0.34;
        irPara(y_ + (height() + Halo - y_) * std::min(1.0, t / 0.66));
    });
}

bool Feixe::eventFilter(QObject *o, QEvent *e)
{
    if (o == parent() && e->type() == QEvent::Resize)
        setGeometry(static_cast<QWidget *>(o)->rect());
    return false;
}

QRect Feixe::faixa(qreal y) const
{
    return QRect(0, int(std::floor(y)) - Halo - 1, width(), Halo * 2 + 3);
}

void Feixe::irPara(qreal y)
{
    const QRect antes = faixa(y_);
    y_ = y;
    update(antes.united(faixa(y_)));
}

void Feixe::comecar()
{
    ativo_ = true;
    fim_.stop();
    opacidade_ = 1;
    if (Tema::reduzirMovimento()) {
        // Sem varrimento: o estado "a ler" diz-se na barra.
        hide();
        return;
    }
    raise();
    show();
    y_ = -Halo * 2;
    ciclo_->start();
}

void Feixe::terminar(const std::function<void()> &depois)
{
    ativo_ = false;
    ciclo_->stop();
    if (Tema::reduzirMovimento() || !isVisible()) {
        hide();
        if (depois)
            depois();
        return;
    }
    fim_.stop();
    disconnect(&fim_, &QVariantAnimation::finished, nullptr, nullptr);
    connect(&fim_, &QVariantAnimation::finished, this, [this, depois] {
        hide();
        if (depois)
            depois();
    });
    fim_.setStartValue(0.0);
    fim_.setEndValue(1.0);
    fim_.setDuration(600);
    fim_.start();
}

void Feixe::paintEvent(QPaintEvent *)
{
    if (y_ < -Halo || y_ > height() + Halo)
        return;
    QPainter p(this);
    p.setOpacity(opacidade_);
    const QColor claude = Tema::cor(Cor::Claude);
    const auto comAlfa = [&claude](qreal a) {
        QColor c = claude;
        c.setAlphaF(a);
        return c;
    };
    // O halo: mais forte junto da linha, a desvanecer para cima e para baixo. Por cima, mais longo (o rasto).
    QLinearGradient g(0, y_ - Halo, 0, y_ + Halo * 0.6);
    g.setColorAt(0, comAlfa(0));
    g.setColorAt(0.62, comAlfa(0.10));
    g.setColorAt(0.625, comAlfa(0.18));
    g.setColorAt(1, comAlfa(0));
    p.fillRect(QRectF(0, y_ - Halo, width(), Halo * 1.6), g);
    // A linha, um pouco mais intensa ao centro.
    QLinearGradient linha(0, 0, width(), 0);
    linha.setColorAt(0, comAlfa(0.35));
    linha.setColorAt(0.5, comAlfa(0.95));
    linha.setColorAt(1, comAlfa(0.35));
    p.fillRect(QRectF(0, y_, width(), 1), linha);
}

// --- PainelEvidencia --------------------------------------------------------------------------------------

PainelEvidencia::PainelEvidencia(QWidget *pai) : QWidget(pai), modelo_(new ModeloEvidencia(this))
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Tema::cor(Cor::Substrato));
    setPalette(pal);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *topo = new QWidget;
    topo->setFixedHeight(36);
    auto *h = new QHBoxLayout(topo);
    h->setContentsMargins(16, 0, 16, 0);
    cabecalho_ = rotuloMeta(QStringLiteral("Evidência"), Tema::cor(Cor::TextoSecundario));
    contexto_ = rotuloMeta(QString(), Tema::cor(Cor::TextoTenue));
    h->addWidget(cabecalho_);
    h->addStretch(1);
    h->addWidget(contexto_);
    v->addWidget(topo);
    auto *linha = new QFrame;
    linha->setFixedHeight(1);
    linha->setStyleSheet(QStringLiteral("background: %1;").arg(Tema::css(Cor::Hairline)));
    v->addWidget(linha);

    lista_ = new QListView;
    lista_->setModel(modelo_);
    lista_->setItemDelegate(new DelegadoEvidencia(modelo_, lista_));
    lista_->setUniformItemSizes(true);
    lista_->setFrameShape(QFrame::NoFrame);
    lista_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lista_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lista_->setSelectionMode(QAbstractItemView::SingleSelection);
    lista_->setMouseTracking(true);
    lista_->setAttribute(Qt::WA_MacShowFocusRect, false);
    lista_->viewport()->setAttribute(Qt::WA_Hover);
    lista_->setAccessibleName(QStringLiteral("Evidência enviada ao Claude"));
    lista_->setStyleSheet(QStringLiteral("QListView { background: %1; outline: none; }").arg(Tema::css(Cor::Substrato)));
    minimapa_ = new Minimapa(modelo_);
    lista_->setVerticalScrollBar(minimapa_);
    lista_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    v->addWidget(lista_, 1);

    feixe_ = new Feixe(lista_->viewport());
    lista_->viewport()->installEventFilter(this);
    for (QVariantAnimation *a : {&ratoAnim_, &atualAnim_}) {
        a->setStartValue(0.0);
        a->setEndValue(1.0);
        a->setEasingCurve(Tema::resposta());
    }
    connect(&ratoAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        modelo_->rato.t = v.toReal();
        for (int i : {modelo_->rato.nova, modelo_->rato.antiga})
            if (i >= 0)
                lista_->viewport()->update(lista_->visualRect(modelo_->index(i)));
    });
    connect(&atualAnim_, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        modelo_->atual.t = v.toReal();
        for (int i : {modelo_->atual.nova, modelo_->atual.antiga})
            if (i >= 0)
                lista_->viewport()->update(lista_->visualRect(modelo_->index(i)));
    });
    connect(lista_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &agora) { mudar(modelo_->atual, atualAnim_, agora.isValid() ? agora.row() : -1); });

    cascata_.setStartValue(0.0);
    cascata_.setEasingCurve(QEasingCurve::OutCubic); // o tempo avança; cada marca tem a sua curva no modelo
    connect(&cascata_, &QVariantAnimation::valueChanged, this, [this](const QVariant &t) {
        modelo_->definirTempoDaCascata(t.toReal());
        lista_->viewport()->update();
        minimapa_->update();
    });

    deslizar_.setEasingCurve(Tema::resposta());
    connect(&deslizar_, &QVariantAnimation::valueChanged, this, [this](const QVariant &y) { minimapa_->setValue(y.toInt()); });

    halo_.setStartValue(1.0);
    halo_.setEndValue(0.0);
    halo_.setEasingCurve(QEasingCurve::InCubic); // fica aceso e só no fim se apaga
}

void PainelEvidencia::mudar(ModeloEvidencia::Transicao &tr, QVariantAnimation &anim, int linha)
{
    if (linha == tr.nova)
        return;
    tr.antiga = tr.nova;
    tr.nova = linha;
    anim.stop();
    anim.setDuration(Tema::duracao(120));
    if (anim.duration() == 0) {
        tr.t = 1;
        lista_->viewport()->update();
        return;
    }
    tr.t = 0;
    anim.start();
}

bool PainelEvidencia::eventFilter(QObject *o, QEvent *e)
{
    if (o == lista_->viewport()) {
        if (e->type() == QEvent::MouseMove || e->type() == QEvent::HoverMove) {
            const QPoint pos = lista_->viewport()->mapFromGlobal(QCursor::pos());
            const QModelIndex i = lista_->indexAt(pos);
            mudar(modelo_->rato, ratoAnim_, i.isValid() ? i.row() : -1);
        } else if (e->type() == QEvent::Leave || e->type() == QEvent::HoverLeave) {
            mudar(modelo_->rato, ratoAnim_, -1);
        }
    }
    return QWidget::eventFilter(o, e);
}

void PainelEvidencia::atualizarCabecalho()
{
    if (modelo_->exemplo()) {
        cabecalho_->setText(QStringLiteral("EVIDÊNCIA · EXEMPLO"));
        contexto_->setText(QStringLiteral("UMA CASA INVENTADA"));
        return;
    }
    cabecalho_->setText(total() ? QStringLiteral("EVIDÊNCIA · %1 LINHAS").arg(total()) : QStringLiteral("EVIDÊNCIA"));
    contexto_->setText(quando_.isEmpty() ? QString() : QStringLiteral("ENVIADA AO CLAUDE %1").arg(quando_.toUpper()));
}

void PainelEvidencia::definirDados(const QJsonObject &dados, const QString &quando)
{
    quando_ = quando;
    modelo_->definir(linhasDosDados(dados), false);
    modelo_->definirTempoDaCascata(1e9);
    minimapa_->recalcular();
    atualizarCabecalho();
}

void PainelEvidencia::mostrarExemplo()
{
    quando_.clear();
    modelo_->definir(linhasDeExemplo(), true);
    // Como ficaria depois de uma triagem: o portátil, o alerta e os logins dele citados.
    modelo_->definirCitadas({3, 7, 9});
    modelo_->definirTempoDaCascata(1e9);
    minimapa_->recalcular();
    atualizarCabecalho();
}

void PainelEvidencia::limpar()
{
    quando_.clear();
    modelo_->definir({}, false);
    minimapa_->recalcular();
    atualizarCabecalho();
}

void PainelEvidencia::marcar(const QVector<int> &citadas, const QHash<int, int> &gravidades)
{
    for (auto it = gravidades.begin(); it != gravidades.end(); ++it)
        modelo_->definirGravidade(it.key(), it.value());
    modelo_->definirCitadas(citadas);
    const int duracao = Tema::duracao(int(citadas.size()) * 40 + 180);
    cascata_.stop();
    if (duracao == 0) {
        modelo_->definirTempoDaCascata(1e9);
    } else {
        modelo_->definirTempoDaCascata(0);
        cascata_.setDuration(duracao);
        cascata_.setEndValue(qreal(duracao));
        cascata_.start();
    }
    minimapa_->recalcular();
    lista_->viewport()->update();
}

void PainelEvidencia::acender(int primeira, int ultima)
{
    if (primeira < 0 || primeira >= total())
        return;
    ultima = std::min(ultima, total() - 1);
    // A primeira linha fica ao centro; o resto do intervalo, se couber, por baixo.
    const int alvo = qBound(minimapa_->minimum(), primeira * Tema::AlturaDaLinha - lista_->viewport()->height() / 2 + Tema::AlturaDaLinha,
                            minimapa_->maximum());
    deslizar_.stop();
    deslizar_.setDuration(Tema::duracao(320));
    deslizar_.setStartValue(minimapa_->value());
    deslizar_.setEndValue(alvo);
    if (deslizar_.duration() == 0)
        minimapa_->setValue(alvo);
    else
        deslizar_.start();

    QSet<int> linhas;
    for (int i = primeira; i <= ultima; ++i)
        linhas << i;
    lista_->setCurrentIndex(modelo_->index(primeira));
    halo_.stop();
    disconnect(&halo_, &QVariantAnimation::valueChanged, nullptr, nullptr);
    connect(&halo_, &QVariantAnimation::valueChanged, this, [this, linhas](const QVariant &v) {
        modelo_->definirHalo(linhas, v.toReal());
        lista_->viewport()->update();
    });
    modelo_->definirHalo(linhas, 1);
    // Com Reduzir movimento o halo não se apaga sozinho: fica até à próxima citação.
    if (!Tema::reduzirMovimento()) {
        halo_.setDuration(1200);
        halo_.start();
    }
    lista_->viewport()->update();
}

void PainelEvidencia::comecarLeitura()
{
    feixe_->comecar();
}

void PainelEvidencia::terminarLeitura(const std::function<void()> &depois)
{
    feixe_->terminar(depois);
}
