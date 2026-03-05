module;
#include <QObject>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Command.SessionManager;

import Command.Session;

export namespace ArtifactCore {

/**
 * @brief Manage the active EditSession.
 * Bridges UI actions and the Command system.
 */
class LIBRARY_DLL_API EditSessionManager : public QObject {
    Q_OBJECT
private:
    EditSession* activeSession_ = nullptr;
    
public:
    static EditSessionManager& instance() {
        static EditSessionManager instance;
        return instance;
    }

    void setActiveSession(EditSession* session) {
        activeSession_ = session;
    }

    EditSession* activeSession() const {
        return activeSession_;
    }

    bool hasActiveSession() const {
        return activeSession_ != nullptr;
    }
};

}
