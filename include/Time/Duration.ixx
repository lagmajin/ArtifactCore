module;
#include <QString>
export module Duration;





export namespace ArtifactCore {

 class DurationPrivate;

 class  Duration {
 private:
  DurationPrivate* const	pDuration_;
 public:
  Duration();
  //explicit Duration(const Time& time);
  //Duration(const Framerate& framerate, const FrameCount& count);
  //Duration(const Duration& duration);
  Duration(Duration&& duration);
  ~Duration();
  //Time duration() const;
  //void setDuration(const Time& duration);
  QString toString() const;
  void setFromString(const QString& str);
  //void setFromFrame(const Framerate& framerate, const FrameCount& count);
  int64_t toSecond() const;
  void setFromSecond(long long int second = 0);
  operator QString() const;

  operator long long int() const;

  void setFromRandom();


  Duration& operator=(long long int sec);
  //Duration& operator=(const Time& duration);
  //Duration& operator=(const PString& str);
  //Duration& operator=(const Duration& duration);
  //Duration& operator=(Duration&& duration);


 };

 bool operator==(const Duration& duration1, const Duration& duration2);
 bool operator!=(const Duration& duration1, const Duration& duration2);

 bool operator<(const Duration& duration1, const Duration& duration2);
 bool operator>(const Duration& duration1, const Duration& duration2);

 bool operator<=(const Duration& duration1, const Duration& duration2);
 bool operator>=(const Duration& duration1, const Duration& duration2);


}