module;
#include <QDateTime>
#include <QString>
#include <QUuid>
#include <utility>


export module Diagnostics.DiagnosticMarker;

export namespace ArtifactCore {

enum class MarkerType { Bug, Fix, Warning, Note };

struct DiagnosticMarker {
  QString id;
  QDateTime timestamp;
  MarkerType type;
  QString message;
  QString location; // e.g., "Layer:123 Frame:456" or JSON for position
  QString author;

  DiagnosticMarker()
      : id(QUuid::createUuid().toString()),
        timestamp(QDateTime::currentDateTime()), type(MarkerType::Note) {}

  DiagnosticMarker(MarkerType t, const QString &msg, const QString &loc,
                   const QString &auth = "")
      : id(QUuid::createUuid().toString()),
        timestamp(QDateTime::currentDateTime()), type(t), message(msg),
        location(loc), author(auth) {}

  // Copy constructor, etc. if needed
};

} // namespace ArtifactCore