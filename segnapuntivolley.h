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

#ifndef SEGNAPUNTIVOLLEY_H
#define SEGNAPUNTIVOLLEY_H

#include <QObject>
#include <QWidget>
#include <QList>
#include <QVector>
#include <QDateTime>
#include <QFileInfoList>
#include <QUrl>

#include "slidewindow.h"
#include "nonetwindow.h"
#include "serverdiscoverer.h"
#include "scorepanel.h"


QT_BEGIN_NAMESPACE
class QSettings;
class QGroupBox;
class QLCDNumber;
class QFile;
class QGridLayout;
QT_END_NAMESPACE


class SegnapuntiVolley : public ScorePanel
{
    Q_OBJECT

public:
    SegnapuntiVolley(QUrl _serverUrl, QFile *_logFile);
    ~SegnapuntiVolley();
    void closeEvent(QCloseEvent *event);

private:
    QSettings         *pSettings;
    QLabel            *team[2];
    QLCDNumber        *score[2];
    QLabel            *scoreLabel;
    QLCDNumber        *set[2];
    QLabel            *setLabel;
    QLabel            *servizio[2];
    QLCDNumber        *timeout[2];
    QLabel            *timeoutLabel;
    QPalette           pal;
    int                iServizio;
    int                iTimeoutFontSize;
    int                iSetFontSize;
    int                iScoreFontSize;
    int                iServiceFontSize;
    int                iTeamFontSize;
    int                maxTeamNameLen;

    void               createPanelElements();
    QGridLayout*       createPanel();

private slots:
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);

protected:
    void buildFontSizes();
};

#endif // SEGNAPUNTIVOLLEY_H
