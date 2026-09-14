#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
#include <QIcon>
#include <QUrlQuery>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);

    QFile styleFile(":/res/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    ui->weekTable->setIconSize(QSize(28, 28));
    ui->hourTable->setIconSize(QSize(24, 24));

    for (QTableWidget *table : {ui->weekTable, ui->hourTable}) {
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->viewport()->setMouseTracking(true);
    }

    ui->weekTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->weekTable->verticalHeader()->setVisible(false);
    ui->weekTable->verticalHeader()->setDefaultSectionSize(52);

    ui->hourTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->hourTable->horizontalHeader()->setDefaultSectionSize(70);
    ui->hourTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->hourTable->verticalHeader()->setDefaultSectionSize(52);

    ui->stackedWidget->setCurrentWidget(ui->week_page);

    m_clearAction = ui->searchInput->addAction(makeCloseIcon(QColor("#8f87a3"), 12),
                                                QLineEdit::TrailingPosition);
    m_clearAction->setVisible(false);
    connect(m_clearAction, &QAction::triggered, ui->searchInput, &QLineEdit::clear);
}

QIcon MainWindow::makeCloseIcon(const QColor &color, int size) const
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color);
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    const int margin = size / 4;
    painter.drawLine(margin, margin, size - margin, size - margin);
    painter.drawLine(size - margin, margin, margin, size - margin);

    return QIcon(pixmap);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_backButton_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->week_page);
}


void MainWindow::on_searchInput_returnPressed()
{
    const QString city = ui->searchInput->text().trimmed();
    if (city.isEmpty())
        return;

    geocodeCity(city);
}

void MainWindow::geocodeCity(const QString &city)
{
    QUrl url("https://geocoding-api.open-meteo.com/v1/search");
    QUrlQuery query;
    query.addQueryItem("name", city);
    query.addQueryItem("count", "1");
    query.addQueryItem("language", "en");
    query.addQueryItem("format", "json");
    url.setQuery(query);

    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Geocoding error:" << reply->errorString();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray results = doc.object().value("results").toArray();
        if (results.isEmpty()) {
            qDebug() << "City not found";
            return;
        }

        const QJsonObject first = results.first().toObject();
        m_currentLat = first.value("latitude").toDouble();
        m_currentLon = first.value("longitude").toDouble();
        m_currentCityName = first.value("name").toString();

        qDebug() << "Found coordinates:" << m_currentLat << m_currentLon;
        fetchWeather(m_currentLat, m_currentLon);

    });
}


void MainWindow::fetchWeather(double lat, double lon)
{
    QUrl url("https://api.open-meteo.com/v1/forecast");
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(lat));
    query.addQueryItem("longitude", QString::number(lon));
    query.addQueryItem("daily", "temperature_2m_max,temperature_2m_min,apparent_temperature_max,weathercode");
    query.addQueryItem("hourly", "temperature_2m,weathercode,is_day");
    query.addQueryItem("timezone", "auto");
    url.setQuery(query);

    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Weather error:" << reply->errorString();
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        m_utcOffsetSeconds = root.value("utc_offset_seconds").toInt();

        const QJsonObject daily = root.value("daily").toObject();
        const QJsonArray dailyMax = daily.value("temperature_2m_max").toArray();
        const QJsonArray dailyMin = daily.value("temperature_2m_min").toArray();
        const QJsonArray dailyFeels = daily.value("apparent_temperature_max").toArray();
        const QJsonArray dailyCode = daily.value("weathercode").toArray();

        m_dailyTempMax.clear();
        m_dailyTempMin.clear();
        m_dailyFeelsLike.clear();
        m_dailyWeatherCode.clear();
        for (int i = 0; i < dailyMax.size(); ++i) {
            m_dailyTempMax.append(dailyMax.at(i).toDouble());
            m_dailyTempMin.append(dailyMin.at(i).toDouble());
            m_dailyFeelsLike.append(dailyFeels.at(i).toDouble());
            m_dailyWeatherCode.append(dailyCode.at(i).toInt());
        }

        const QJsonObject hourly = root.value("hourly").toObject();
        const QJsonArray hourlyTemp = hourly.value("temperature_2m").toArray();
        const QJsonArray hourlyCode = hourly.value("weathercode").toArray();
        const QJsonArray hourlyIsDay = hourly.value("is_day").toArray();

        m_hourlyTemp.clear();
        m_hourlyIsDay.clear();
        m_hourlyWeatherCode.clear();
        for (int i = 0; i < hourlyTemp.size(); ++i) {
            m_hourlyTemp.append(hourlyTemp.at(i).toDouble());
            m_hourlyWeatherCode.append(hourlyCode.at(i).toInt());
            m_hourlyIsDay.append(hourlyIsDay.at(i).toInt());
        }

        fill_weekTable();
    });
}



void MainWindow::fill_weekTable()
{
    ui->weekTable->setHorizontalHeaderLabels({"Day", "Condition", "Temp", "Feels like"});

    m_weekDates.clear();
    const QDate today = QDate::currentDate();

    for (int row = 0; row < ui->weekTable->rowCount() && row < m_dailyTempMax.size(); ++row) {
        const QDate day = today.addDays(row);
        m_weekDates.append(day);

        const QString dayLabel = (row == 0) ? "Today" : day.toString("ddd");
        //const QString condition = weatherCodeToText(m_dailyWeatherCode.at(row));
        auto *conditionItem = new QTableWidgetItem();
        if (row == 0 && !m_hourlyWeatherCode.isEmpty()) {
            const int idx = qBound(0, currentHourIndex(), m_hourlyWeatherCode.size() - 1);
            conditionItem->setIcon(weatherCodeToIcon(m_hourlyWeatherCode.at(idx), m_hourlyIsDay.at(idx) != 0));
        } else {
            conditionItem->setIcon(weatherCodeToIcon(m_dailyWeatherCode.at(row), true));
        }

        const int tempHigh = qRound(m_dailyTempMax.at(row));
        const int tempLow = qRound(m_dailyTempMin.at(row));
        const int feelsLike = qRound(m_dailyFeelsLike.at(row));

        ui->weekTable->setItem(row, 0, new QTableWidgetItem(dayLabel));
        //ui->weekTable->setItem(row, 1, new QTableWidgetItem(condition));
        ui->weekTable->setItem(row, 1, conditionItem);
        ui->weekTable->setItem(row, 2, new QTableWidgetItem(QString("%1° / %2°").arg(tempLow).arg(tempHigh)));
        ui->weekTable->setItem(row, 3, new QTableWidgetItem(QString("%1°").arg(feelsLike)));
    }
}


void MainWindow::on_weekTable_cellClicked(int row, int column)
{
    if (row < 0 || row >= m_weekDates.size())
        return;

    set_dayTitleLabel(row);
    fill_hourTable(row);
    ui->stackedWidget->setCurrentWidget(ui->day_page);
}

void MainWindow::fill_hourTable(int row)
{
    if (row < 0 || row >= m_weekDates.size())
        return;

    ui->hourTable->setVerticalHeaderLabels({"Condition", "Temp"});
    ui->cityName->setText(m_currentCityName);

    const int startHour = row * 24;
    for (int hour = 0; hour < ui->hourTable->columnCount(); ++hour) {
        const int index = startHour + hour;
        if (index >= m_hourlyTemp.size())
            break;

        ui->hourTable->setItem(1, hour, new QTableWidgetItem(QString("%1°").arg(qRound(m_hourlyTemp.at(index)))));
        //ui->hourTable->setItem(1, hour, new QTableWidgetItem(weatherCodeToText(m_hourlyWeatherCode.at(index))));
        auto *conditionItem = new QTableWidgetItem();
        conditionItem->setIcon(weatherCodeToIcon(m_hourlyWeatherCode.at(index), m_hourlyIsDay.at(index) != 0));
        ui->hourTable->setItem(0, hour, conditionItem);
    }
}

int MainWindow::currentHourIndex() const
{
    const QDateTime cityNow = QDateTime::currentDateTimeUtc().addSecs(m_utcOffsetSeconds);
    return cityNow.time().hour();
}

QString MainWindow::weatherCodeToText(int code) const
{
    if (code == 0) return "Sunny";
    if (code <= 3) return "Cloudy";
    if (code == 45 || code == 48) return "Foggy";
    if (code >= 51 && code <= 67) return "Rainy";
    if (code >= 71 && code <= 77) return "Snowy";
    if (code >= 80 && code <= 82) return "Rainy";
    return "Stormy"; // 95+ - гроза
}

QIcon MainWindow::weatherCodeToIcon(int code, bool isDay) const
{
    if (isDay) {
        if (code == 0) return QIcon(":/res/img/sunny.png");
        if (code <= 3) return QIcon(":/res/img/cloudy.png");
        if (code == 45 || code == 48) return QIcon(":/res/img/foggy.png");
        if (code >= 51 && code <= 67) return QIcon(":/res/img/rainy.png");
        if (code >= 71 && code <= 77) return QIcon(":/res/img/snowy.png");
        if (code >= 80 && code <= 82) return QIcon(":/res/img/very_rainy.png");
        return QIcon(":/res/img/stormy.png");
    } else {
        if (code == 0) return QIcon(":/res/img/clear_night.png");
        if (code <= 3) return QIcon(":/res/img/foggy_night.png");
        if (code == 45 || code == 48) return QIcon(":/res/img/foggy_night.png");
        if (code >= 51 && code <= 67) return QIcon(":/res/img/rainy_night.png");
        if (code >= 71 && code <= 77) return QIcon(":/res/img/snowy_night.png");
        if (code >= 80 && code <= 82) return QIcon(":/res/img/rainy_night.png");
        return QIcon(":/res/img/stormy_night.png");

    }
}

void MainWindow::set_dayTitleLabel(int row) {
    if (row < 0 || row >= m_weekDates.size())
        return;

    const QDate day = m_weekDates.at(row);
    const QString title = (row == 0) ? "Today, " + day.toString("d MMMM") : day.toString("dddd, d MMMM");
    ui->dayTitleLabel->setText(title);
}

void MainWindow::on_searchInput_textChanged(const QString &text)
{
    if (m_clearAction)
        m_clearAction->setVisible(!text.isEmpty());

    if (text.isEmpty()) {
        ui->weekTable->clearContents();
        m_weekDates.clear();
        m_dailyTempMax.clear();
        m_dailyTempMin.clear();
        m_dailyFeelsLike.clear();
        m_dailyWeatherCode.clear();
        m_hourlyTemp.clear();
        m_hourlyWeatherCode.clear();
        m_currentCityName.clear();
    }

}

