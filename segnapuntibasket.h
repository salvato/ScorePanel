/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#ifndef SEGNAPUNTIBASKET_H
#define SEGNAPUNTIBASKET_H

#include <QObject>
#include <QWidget>
#include <QList>
#include <QVector>
#include <QDateTime>
#include <QFileInfoList>
#include <QSerialPort>
#include <QUrl>


#include "slidewindow.h"
#include "serverdiscoverer.h"
#include "timedscorepanel.h"


QT_BEGIN_NAMESPACE
class QSettings;
class QGroupBox;
class QLCDNumber;
class QFile;
class QGridLayout;
class QLayoutItem;
QT_END_NAMESPACE


class SegnapuntiBasket : public TimedScorePanel
{
private:
    Q_OBJECT

public:
    explicit SegnapuntiBasket(QUrl _serverUrl, QFile *_logFile);
    ~SegnapuntiBasket();
    void closeEvent(QCloseEvent *event);

private:
    QLabel            *team[2];
    QLCDNumber        *score[2];
    QLCDNumber        *period;
    QLCDNumber        *teamFouls[2];
    QLabel            *timeLabel;
    QLabel            *timeout[2];
    QLabel            *bonus[2];
    QLabel            *possess[2];
    QLabel            *foulsLabel;
    QSettings         *pSettings;
    QPalette           pal;
    int                iPossess;
    int                maxTeamNameLen;
    int                iTimeoutFontSize;
    int                iTimeFontSize;
    int                iTeamFontSize;
    int                iTeamFoulsFontSize;
    int                iBonusFontSize;

public slots:
    void resizeEvent(QResizeEvent *event);
    void onNewTimeValue(QString sTimeValue);

private slots:
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);
    void onArduinoFound();

protected:
    void                   buildFontSizes();
    void                   createPanelElements();
    QGridLayout           *createPanel();
};

#endif // SEGNAPUNTIBASKET_H
