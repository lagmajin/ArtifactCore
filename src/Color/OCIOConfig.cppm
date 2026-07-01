module;

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>

module Color.OCIOConfig;

namespace ArtifactCore {

// ---------------------------------------------------------------------------
// PIMPL implementation
// ---------------------------------------------------------------------------
class OCIOConfig::Impl {
public:
    QVector<OCIORole> roles_;
    QVector<OCIOColorSpace> colorSpaces_;
    QVector<OCIDisplayView> displayViews_;
    QString activeDisplay_;
    QString activeView_;
    QString activeLooks_;
    QString workingSpace_;
    QString configFilePath_;
    QString lastError_;
    QMap<QString, int> colorSpaceIndex_;

    void rebuildIndex() {
        colorSpaceIndex_.clear();
        for (int i = 0; i < colorSpaces_.size(); ++i) {
            colorSpaceIndex_[colorSpaces_[i].name] = i;
            for (const auto& alias : colorSpaces_[i].aliases) {
                colorSpaceIndex_[alias] = i;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
OCIOConfig::OCIOConfig()
    : impl_(new Impl())
{
}

OCIOConfig::OCIOConfig(const QString& configFilePath)
    : impl_(new Impl())
{
    loadFromFile(configFilePath);
}

OCIOConfig::~OCIOConfig()
{
    delete impl_;
}

// ---------------------------------------------------------------------------
// Load / Save
// ---------------------------------------------------------------------------
bool OCIOConfig::loadFromFile(const QString& path)
{
    impl_->configFilePath_ = path;
    impl_->lastError_.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        impl_->lastError_ = QStringLiteral("Cannot open config file: %1").arg(path);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        impl_->lastError_ = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        impl_->lastError_ = QStringLiteral("Config file is not a JSON object");
        return false;
    }

    return loadFromJson(doc.object());
}

bool OCIOConfig::loadFromJson(const QJsonObject& json)
{
    impl_->lastError_.clear();

    // Parse roles
    impl_->roles_.clear();
    QJsonObject rolesObj = json.value(QStringLiteral("roles")).toObject();
    for (auto it = rolesObj.begin(); it != rolesObj.end(); ++it) {
        OCIORole role;
        role.roleName = it.key();
        role.colorSpaceName = it.value().toString();
        impl_->roles_.append(role);
    }

    // Parse color spaces
    impl_->colorSpaces_.clear();
    QJsonArray csArray = json.value(QStringLiteral("color_spaces")).toArray();
    for (const QJsonValue& val : csArray) {
        QJsonObject obj = val.toObject();
        OCIOColorSpace cs;
        cs.name = obj.value(QStringLiteral("name")).toString();
        cs.family = obj.value(QStringLiteral("family")).toString();
        cs.description = obj.value(QStringLiteral("description")).toString();
        cs.isData = obj.value(QStringLiteral("is_data")).toBool(false);

        QJsonArray aliasArray = obj.value(QStringLiteral("aliases")).toArray();
        for (const QJsonValue& aliasVal : aliasArray) {
            cs.aliases.append(aliasVal.toString());
        }

        impl_->colorSpaces_.append(cs);
    }
    impl_->rebuildIndex();

    // Parse display views
    impl_->displayViews_.clear();
    QJsonArray dvArray = json.value(QStringLiteral("display_views")).toArray();
    for (const QJsonValue& val : dvArray) {
        QJsonObject obj = val.toObject();
        OCIDisplayView dv;
        dv.display = obj.value(QStringLiteral("display")).toString();
        dv.view = obj.value(QStringLiteral("view")).toString();
        dv.colorspace = obj.value(QStringLiteral("colorspace")).toString();
        dv.looks = obj.value(QStringLiteral("looks")).toString();
        impl_->displayViews_.append(dv);
    }

    // Active configuration
    impl_->activeDisplay_ = json.value(QStringLiteral("active_display")).toString();
    impl_->activeView_ = json.value(QStringLiteral("active_view")).toString();
    impl_->activeLooks_ = json.value(QStringLiteral("active_looks")).toString();
    impl_->workingSpace_ = json.value(QStringLiteral("working_space")).toString();

    return true;
}

QJsonObject OCIOConfig::toJson() const
{
    QJsonObject json;

    // Roles
    QJsonObject rolesObj;
    for (const auto& role : impl_->roles_) {
        rolesObj[role.roleName] = role.colorSpaceName;
    }
    json[QStringLiteral("roles")] = rolesObj;

    // Color spaces
    QJsonArray csArray;
    for (const auto& cs : impl_->colorSpaces_) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = cs.name;
        obj[QStringLiteral("family")] = cs.family;
        obj[QStringLiteral("description")] = cs.description;
        obj[QStringLiteral("is_data")] = cs.isData;

        QJsonArray aliasArray;
        for (const auto& alias : cs.aliases) {
            aliasArray.append(alias);
        }
        obj[QStringLiteral("aliases")] = aliasArray;
        csArray.append(obj);
    }
    json[QStringLiteral("color_spaces")] = csArray;

    // Display views
    QJsonArray dvArray;
    for (const auto& dv : impl_->displayViews_) {
        QJsonObject obj;
        obj[QStringLiteral("display")] = dv.display;
        obj[QStringLiteral("view")] = dv.view;
        obj[QStringLiteral("colorspace")] = dv.colorspace;
        obj[QStringLiteral("looks")] = dv.looks;
        dvArray.append(obj);
    }
    json[QStringLiteral("display_views")] = dvArray;

    // Active config
    if (!impl_->activeDisplay_.isEmpty())
        json[QStringLiteral("active_display")] = impl_->activeDisplay_;
    if (!impl_->activeView_.isEmpty())
        json[QStringLiteral("active_view")] = impl_->activeView_;
    if (!impl_->activeLooks_.isEmpty())
        json[QStringLiteral("active_looks")] = impl_->activeLooks_;
    if (!impl_->workingSpace_.isEmpty())
        json[QStringLiteral("working_space")] = impl_->workingSpace_;

    return json;
}

// ---------------------------------------------------------------------------
// Built-in ACES config (no external OCIO library required)
// ---------------------------------------------------------------------------
OCIOConfig OCIOConfig::createACESConfig()
{
    OCIOConfig config;

    // --- Standard ACES color spaces ---
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("ACES2065-1");
        cs.family = QStringLiteral("ACES");
        cs.description = QStringLiteral("ACES2065-1 AP0 color space");
        cs.aliases.append(QStringLiteral("aces"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("ACEScg");
        cs.family = QStringLiteral("ACES");
        cs.description = QStringLiteral("ACEScg AP1 color space (scene linear)");
        cs.aliases.append(QStringLiteral("acescg"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("ACEScc");
        cs.family = QStringLiteral("ACES");
        cs.description = QStringLiteral("ACEScc logarithmic color space");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("ACEScct");
        cs.family = QStringLiteral("ACES");
        cs.description = QStringLiteral("ACEScct logarithmic (ARRI style toe)");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("sRGB");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("sRGB display (IEC 61966-2-1)");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Rec.709");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("ITU-R BT.709 display");
        cs.aliases.append(QStringLiteral("BT.709"));
        cs.aliases.append(QStringLiteral("rec709"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Rec.2020");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("ITU-R BT.2020 display");
        cs.aliases.append(QStringLiteral("BT.2020"));
        cs.aliases.append(QStringLiteral("rec2020"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("P3-D65");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("Display P3 with D65 white point");
        cs.aliases.append(QStringLiteral("Display P3"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("P3-D60");
        cs.family = QStringLiteral("ACES");
        cs.description = QStringLiteral("P3 with ACES (D60) white point");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Linear");
        cs.family = QStringLiteral("Generic");
        cs.description = QStringLiteral("Generic linear sRGB (no transfer)");
        config.impl_->colorSpaces_.append(cs);
    }

    config.impl_->rebuildIndex();

    // --- Roles ---
    {
        OCIORole role;
        role.roleName = QStringLiteral("scene_linear");
        role.colorSpaceName = QStringLiteral("ACEScg");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("color_timing");
        role.colorSpaceName = QStringLiteral("ACEScct");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("display");
        role.colorSpaceName = QStringLiteral("sRGB");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("default_byte");
        role.colorSpaceName = QStringLiteral("sRGB");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("default_float");
        role.colorSpaceName = QStringLiteral("Linear");
        config.impl_->roles_.append(role);
    }

    // --- Display Views ---
    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("sRGB");
        dv.view = QStringLiteral("ACES 1.0 SDR Video");
        dv.colorspace = QStringLiteral("sRGB");
        dv.looks = QStringLiteral("ACES 1.0 Default");
        config.impl_->displayViews_.append(dv);
    }
    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("Rec.709");
        dv.view = QStringLiteral("ACES 1.0 SDR Video");
        dv.colorspace = QStringLiteral("Rec.709");
        config.impl_->displayViews_.append(dv);
    }
    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("P3-D65");
        dv.view = QStringLiteral("ACES 1.0 SDR Video");
        dv.colorspace = QStringLiteral("P3-D65");
        config.impl_->displayViews_.append(dv);
    }
    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("Rec.2020");
        dv.view = QStringLiteral("ACES 1.0 SDR Video");
        dv.colorspace = QStringLiteral("Rec.2020");
        config.impl_->displayViews_.append(dv);
    }

    // --- Default active ---
    config.impl_->activeDisplay_ = QStringLiteral("sRGB");
    config.impl_->activeView_ = QStringLiteral("ACES 1.0 SDR Video");
    config.impl_->workingSpace_ = QStringLiteral("ACEScg");

    return config;
}

// ---------------------------------------------------------------------------
// Built-in sRGB config
// ---------------------------------------------------------------------------
OCIOConfig OCIOConfig::createSRGBConfig()
{
    OCIOConfig config;

    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("sRGB");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("sRGB display (IEC 61966-2-1)");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Linear");
        cs.family = QStringLiteral("Generic");
        cs.description = QStringLiteral("Linear sRGB");
        config.impl_->colorSpaces_.append(cs);
    }

    config.impl_->rebuildIndex();

    {
        OCIORole role;
        role.roleName = QStringLiteral("scene_linear");
        role.colorSpaceName = QStringLiteral("Linear");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("display");
        role.colorSpaceName = QStringLiteral("sRGB");
        config.impl_->roles_.append(role);
    }

    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("sRGB");
        dv.view = QStringLiteral("Standard");
        dv.colorspace = QStringLiteral("sRGB");
        config.impl_->displayViews_.append(dv);
    }

    config.impl_->activeDisplay_ = QStringLiteral("sRGB");
    config.impl_->activeView_ = QStringLiteral("Standard");
    config.impl_->workingSpace_ = QStringLiteral("Linear");

    return config;
}

// ---------------------------------------------------------------------------
// Built-in Rec.709 config
// ---------------------------------------------------------------------------
OCIOConfig OCIOConfig::createRec709Config()
{
    OCIOConfig config;

    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Rec.709");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("ITU-R BT.709 display");
        cs.aliases.append(QStringLiteral("BT.709"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Linear");
        cs.family = QStringLiteral("Generic");
        cs.description = QStringLiteral("Linear Rec.709 (scene linear)");
        config.impl_->colorSpaces_.append(cs);
    }

    config.impl_->rebuildIndex();

    {
        OCIORole role;
        role.roleName = QStringLiteral("scene_linear");
        role.colorSpaceName = QStringLiteral("Linear");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("display");
        role.colorSpaceName = QStringLiteral("Rec.709");
        config.impl_->roles_.append(role);
    }

    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("Rec.709");
        dv.view = QStringLiteral("Standard");
        dv.colorspace = QStringLiteral("Rec.709");
        config.impl_->displayViews_.append(dv);
    }

    config.impl_->activeDisplay_ = QStringLiteral("Rec.709");
    config.impl_->activeView_ = QStringLiteral("Standard");
    config.impl_->workingSpace_ = QStringLiteral("Linear");

    return config;
}

// ---------------------------------------------------------------------------
// Built-in Rec.2020 config
// ---------------------------------------------------------------------------
OCIOConfig OCIOConfig::createRec2020Config()
{
    OCIOConfig config;

    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Rec.2020");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("ITU-R BT.2020 display (HDR)");
        cs.aliases.append(QStringLiteral("BT.2020"));
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("Rec.2020 Linear");
        cs.family = QStringLiteral("Generic");
        cs.description = QStringLiteral("Linear Rec.2020 (scene linear for HDR)");
        config.impl_->colorSpaces_.append(cs);
    }
    {
        OCIOColorSpace cs;
        cs.name = QStringLiteral("sRGB");
        cs.family = QStringLiteral("Output");
        cs.description = QStringLiteral("sRGB fallback display");
        config.impl_->colorSpaces_.append(cs);
    }

    config.impl_->rebuildIndex();

    {
        OCIORole role;
        role.roleName = QStringLiteral("scene_linear");
        role.colorSpaceName = QStringLiteral("Rec.2020 Linear");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("display");
        role.colorSpaceName = QStringLiteral("Rec.2020");
        config.impl_->roles_.append(role);
    }
    {
        OCIORole role;
        role.roleName = QStringLiteral("default_byte");
        role.colorSpaceName = QStringLiteral("sRGB");
        config.impl_->roles_.append(role);
    }

    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("Rec.2020");
        dv.view = QStringLiteral("HDR PQ");
        dv.colorspace = QStringLiteral("Rec.2020");
        dv.looks = QStringLiteral("SDR to HDR");
        config.impl_->displayViews_.append(dv);
    }
    {
        OCIDisplayView dv;
        dv.display = QStringLiteral("sRGB");
        dv.view = QStringLiteral("SDR Reference");
        dv.colorspace = QStringLiteral("sRGB");
        config.impl_->displayViews_.append(dv);
    }

    config.impl_->activeDisplay_ = QStringLiteral("Rec.2020");
    config.impl_->activeView_ = QStringLiteral("HDR PQ");
    config.impl_->workingSpace_ = QStringLiteral("Rec.2020 Linear");

    return config;
}

// ---------------------------------------------------------------------------
// Role queries
// ---------------------------------------------------------------------------
QString OCIOConfig::colorSpaceForRole(const QString& role) const
{
    for (const auto& r : impl_->roles_) {
        if (r.roleName == role)
            return r.colorSpaceName;
    }
    return {};
}

void OCIOConfig::setRole(const QString& role, const QString& colorSpace)
{
    for (auto& r : impl_->roles_) {
        if (r.roleName == role) {
            r.colorSpaceName = colorSpace;
            return;
        }
    }
    OCIORole r;
    r.roleName = role;
    r.colorSpaceName = colorSpace;
    impl_->roles_.append(r);
}

QVector<OCIORole> OCIOConfig::roles() const
{
    return impl_->roles_;
}

// ---------------------------------------------------------------------------
// Color space queries
// ---------------------------------------------------------------------------
const OCIOColorSpace* OCIOConfig::colorSpace(const QString& name) const
{
    auto it = impl_->colorSpaceIndex_.find(name);
    if (it != impl_->colorSpaceIndex_.end()) {
        return &impl_->colorSpaces_[it.value()];
    }
    for (const auto& cs : impl_->colorSpaces_) {
        if (cs.name == name)
            return &cs;
        for (const auto& alias : cs.aliases) {
            if (alias == name)
                return &cs;
        }
    }
    return nullptr;
}

QVector<OCIOColorSpace> OCIOConfig::colorSpaces() const
{
    return impl_->colorSpaces_;
}

void OCIOConfig::addColorSpace(const OCIOColorSpace& cs)
{
    impl_->colorSpaces_.append(cs);
    impl_->rebuildIndex();
}

// ---------------------------------------------------------------------------
// Display / View
// ---------------------------------------------------------------------------
QVector<OCIDisplayView> OCIOConfig::displayViews() const
{
    return impl_->displayViews_;
}

void OCIOConfig::addDisplayView(const OCIDisplayView& dv)
{
    impl_->displayViews_.append(dv);
}

QStringList OCIOConfig::displays() const
{
    QStringList result;
    for (const auto& dv : impl_->displayViews_) {
        if (!result.contains(dv.display))
            result.append(dv.display);
    }
    return result;
}

QStringList OCIOConfig::viewsForDisplay(const QString& display) const
{
    QStringList result;
    for (const auto& dv : impl_->displayViews_) {
        if (dv.display == display)
            result.append(dv.view);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Active configuration
// ---------------------------------------------------------------------------
QString OCIOConfig::activeDisplay() const { return impl_->activeDisplay_; }
void OCIOConfig::setActiveDisplay(const QString& display) { impl_->activeDisplay_ = display; }
QString OCIOConfig::activeView() const { return impl_->activeView_; }
void OCIOConfig::setActiveView(const QString& view) { impl_->activeView_ = view; }
QString OCIOConfig::activeLooks() const { return impl_->activeLooks_; }
void OCIOConfig::setActiveLooks(const QString& looks) { impl_->activeLooks_ = looks; }

// ---------------------------------------------------------------------------
// Working space
// ---------------------------------------------------------------------------
QString OCIOConfig::workingSpace() const { return impl_->workingSpace_; }
void OCIOConfig::setWorkingSpace(const QString& cs) { impl_->workingSpace_ = cs; }

// ---------------------------------------------------------------------------
// Config path
// ---------------------------------------------------------------------------
QString OCIOConfig::configFilePath() const { return impl_->configFilePath_; }
void OCIOConfig::setConfigFilePath(const QString& path) { impl_->configFilePath_ = path; }

// ---------------------------------------------------------------------------
// Error
// ---------------------------------------------------------------------------
QString OCIOConfig::lastError() const { return impl_->lastError_; }

} // namespace ArtifactCore

