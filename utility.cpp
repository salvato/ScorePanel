#include "utility.h"

QString
XML_Parse(QString input_string, QString token) {
    // simple XML parser
    //   XML_Parse("<score1>10</score1>","beam")   will return "10"
    // returns "" on error
    QString start_token, end_token, result = QString("NoData");
    start_token = "<" + token + ">";
    end_token = "</" + token + ">";

    int start_pos=-1, end_pos=-1, len=0;

    start_pos = input_string.indexOf(start_token);
    end_pos   = input_string.indexOf(end_token);

    if(start_pos < 0 || end_pos < 0) {
        result = QString("NoData");
    } else {
        start_pos += start_token.length();
        len = end_pos - start_pos;
        if(len>0)
            result = input_string.mid(start_pos, len);
        else
            result = "";
    }

    return result;
}

void
logMessage(QFile *logFile, QString sFunctionName, QString sMessage) {
    Q_UNUSED(sFunctionName)
    Q_UNUSED(sMessage)

    QDateTime dateTime;

    QString sDebugMessage = dateTime.currentDateTime().toString() +
                            sFunctionName +
                            sMessage;
    if(logFile) {
      logFile->write(sDebugMessage.toUtf8().data());
      logFile->write("\n");
      logFile->flush();
    }
    else {
        qDebug() << sDebugMessage;
    }
}


