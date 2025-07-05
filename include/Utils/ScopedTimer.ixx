module;
#include <QElapsedTimer>
#include <QDebug>
#include <QString>
export module Utils.Timer;



export namespace ArtifactCore {

 class ScopedTimer
 {
 public:
  explicit ScopedTimer(const QString& name = "ScopedTimer")
   : m_name(name)
  {
   m_timer.start();
  }

  ~ScopedTimer()
  {
   qDebug() << m_name << "elapsed:"
	<< m_timer.nsecsElapsed() / 1'000'000.0 << "ms"; // ¬”“_‚Â‚«
  }

 private:
  QString m_name;
  QElapsedTimer m_timer;
 };










};