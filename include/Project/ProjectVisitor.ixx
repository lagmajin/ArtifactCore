module;

#include <QString>
#include <QVector>
#include <utility>

export module Project.ProjectVisitor;

import Project.MetadataCollector;

export namespace ArtifactCore {

class ProjectVisitor {
public:
    void beginProject(const QString& id, const QString& name) {
        append(MetadataNodeKind::Project, id, name, QStringLiteral("Project"), 0);
    }

    void visitComposition(const QString& id, const QString& name) {
        append(MetadataNodeKind::Composition, id, name,
               QStringLiteral("Composition"), 1);
    }

    void visitLayer(const QString& id, const QString& name, const QString& type) {
        append(MetadataNodeKind::Layer, id, name, type, 2);
    }

    void visitEffect(const QString& id, const QString& name, const QString& type) {
        append(MetadataNodeKind::Effect, id, name, type, 3);
    }

    void visitProperty(const QString& id, const QString& name, const QString& type,
                       const QVariant& value = QVariant()) {
        MetadataNode node{MetadataNodeKind::Property, id, name, type, 4};
        node.value = value;
        nodes_.push_back(std::move(node));
    }

    void collect(const QVector<MetadataCollector*>& collectors) const {
        MetadataCollectorDriver::visit(nodes_, collectors);
    }

    const QVector<MetadataNode>& nodes() const { return nodes_; }
    void clear() { nodes_.clear(); }

private:
    void append(MetadataNodeKind kind, const QString& id, const QString& name,
                const QString& type, int depth) {
        nodes_.push_back(MetadataNode{kind, id, name, type, depth});
    }

    QVector<MetadataNode> nodes_;
};

} // namespace ArtifactCore
