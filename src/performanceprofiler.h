/*
 * PerformanceProfiler – lightweight instrumentation for desktop app performance
 *
 * Usage:
 *   {
 *     PERF_SCOPE("MainWindow::loadLibrary");
 *     // ... slow work ...
 *   }
 */

#pragma once

#include <QString>
#include <QElapsedTimer>
#include <QDebug>
#include <QHash>
#include <QMutex>

class PerformanceProfiler {
public:
    static PerformanceProfiler* instance() {
        static PerformanceProfiler i;
        return &i;
    }

    void logTiming(const QString &name, qint64 ms) {
        if (ms > 100) { // Only log slow operations
             qDebug() << "[PERF]" << name << "took" << ms << "ms";
        }
        
        QMutexLocker locker(&m_mutex);
        m_timings[name] = ms;
    }

private:
    PerformanceProfiler() = default;
    QHash<QString, qint64> m_timings;
    QMutex m_mutex;
};

class PerformanceScope {
public:
    PerformanceScope(const QString &name) : m_name(name) {
        m_timer.start();
    }
    ~PerformanceScope() {
        PerformanceProfiler::instance()->logTiming(m_name, m_timer.elapsed());
    }
private:
    QString m_name;
    QElapsedTimer m_timer;
};

#define PERF_SCOPE(name) PerformanceScope _perf_scope(name)
