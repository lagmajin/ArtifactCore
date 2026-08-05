module;

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QtGlobal>

export module Localization.LocaleFormatting;

export namespace ArtifactCore {

/** Locale-aware presentation helpers; frame/timecode formatting stays stable. */
class LocaleFormatting {
public:
    static QString formatNumber(double value, int decimals = 2,
                                const QLocale& locale = QLocale())
    {
        return locale.toString(value, 'f', decimals);
    }

    static QString formatPercentage(double value, int decimals = 1,
                                    const QLocale& locale = QLocale())
    {
        return formatNumber(value * 100.0, decimals, locale) + QStringLiteral(" %");
    }

    static QString formatFileSize(qint64 bytes, const QLocale& locale = QLocale())
    {
        if (bytes < 1024) {
            return locale.toString(bytes) + QStringLiteral(" B");
        }
        const QStringList units = {QStringLiteral("KiB"), QStringLiteral("MiB"),
                                   QStringLiteral("GiB"), QStringLiteral("TiB")};
        double value = static_cast<double>(bytes);
        int unit = -1;
        do {
            value /= 1024.0;
            ++unit;
        } while (value >= 1024.0 && unit + 1 < units.size());
        return locale.toString(value, 'f', value >= 10.0 ? 1 : 2) +
               QStringLiteral(" ") + units.at(unit);
    }

    static QString formatFrame(qint64 frame, const QLocale& = QLocale())
    {
        return QString::number(frame);
    }

    static QString formatTimecode(double seconds, double fps)
    {
        if (fps <= 0.0) {
            return QStringLiteral("00:00:00:00");
        }
        const qint64 totalFrames = qMax<qint64>(0, qRound64(seconds * fps));
        const qint64 frames = totalFrames % qMax<qint64>(1, qRound64(fps));
        const qint64 totalSeconds = totalFrames / qMax<qint64>(1, qRound64(fps));
        const qint64 secs = totalSeconds % 60;
        const qint64 minutes = (totalSeconds / 60) % 60;
        const qint64 hours = totalSeconds / 3600;
        return QStringLiteral("%1:%2:%3:%4")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'))
            .arg(frames, 2, 10, QLatin1Char('0'));
    }

    static QString formatDate(const QDate& date, const QLocale& locale = QLocale())
    {
        return locale.toString(date, QLocale::ShortFormat);
    }

    static QString formatDateTime(const QDateTime& dateTime,
                                  const QLocale& locale = QLocale())
    {
        return locale.toString(dateTime, QLocale::ShortFormat);
    }

    static QString formatDuration(qint64 milliseconds)
    {
        const qint64 totalSeconds = qMax<qint64>(0, milliseconds / 1000);
        return QStringLiteral("%1:%2:%3")
            .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
            .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
    }
};

} // namespace ArtifactCore
