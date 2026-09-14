#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QVector>
#include <QIcon>
#include <QColor>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QNetworkAccessManager;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_backButton_clicked();

    void on_searchInput_returnPressed();

    void on_weekTable_cellClicked(int row, int column);

    void fill_weekTable();

    void set_dayTitleLabel(int row);

    void fill_hourTable(int row);

    void on_searchInput_textChanged(const QString &text);

private:
    void geocodeCity(const QString &city);
    QIcon makeCloseIcon(const QColor &color, int size) const;

    Ui::MainWindow *ui;
    QVector<QDate> m_weekDates; // m_weekDates[row] = дата строки weekTable

    QNetworkAccessManager *m_networkManager;
    double m_currentLat = 0.0;
    double m_currentLon = 0.0;
    int m_utcOffsetSeconds = 0;


    void fetchWeather(double lat, double lon);
    QString weatherCodeToText(int code) const;
    QIcon weatherCodeToIcon(int code, bool isDay) const;
    int currentHourIndex() const;


    QVector<double> m_dailyTempMax;
    QVector<double> m_dailyTempMin;
    QVector<double> m_dailyFeelsLike;
    QVector<int> m_dailyWeatherCode;
    QString m_currentCityName;

    QVector<double> m_hourlyTemp;
    QVector<int> m_hourlyIsDay;
    QVector<int> m_hourlyWeatherCode;

    QAction *m_clearAction = nullptr;
};
#endif // MAINWINDOW_H
