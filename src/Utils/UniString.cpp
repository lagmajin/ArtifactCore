module;

#include <QString>

module UniString;


namespace ArtifactCore {

 class UniString::Impl{
 private:
  
 public:
  Impl(); 
  Impl(const Impl& other);
  ~Impl();
  QString str_;
 };

 UniString::Impl::Impl()
 {

 }

 UniString::Impl::Impl(const Impl& other) : str_(other.str_)
 {

 }

 UniString::Impl::~Impl()
 {

 }

 UniString::UniString() :impl_(new Impl())
 {

 }

 UniString::UniString(const UniString& other) :impl_(new Impl())
 {

 }

 UniString::~UniString()
 {
  delete impl_;
 }

 QString UniString::toQString() const
 {
  return QString();
 }

 void UniString::setQString(const QString& str)
 {

 }

 UniString& UniString::operator=(const UniString& other)
 {
  return *this;
 }

 UniString& UniString::operator=(UniString&& other) noexcept
 {
  return *this;
 }

 std::u16string UniString::toStdU16String() const
 {
  return std::u16string();
 }

};