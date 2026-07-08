module;
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <new>
#include <string>
#include <string_view>

export module Core.ArtifactString;

export namespace ArtifactCore {

class StringView {
public:
    StringView() = default;
    StringView(const char* s, size_t len) : data_(s), len_(len) {}
    StringView(const char* s) : data_(s), len_(s ? std::strlen(s) : 0) {}
    size_t length() const { return len_; }
    bool isEmpty() const { return len_ == 0; }
    const char* data() const { return data_ ? data_ : ""; }
    char at(size_t i) const { return (i < len_) ? data_[i] : 0; }
    const char* begin() const { return data(); }
    const char* end() const { return data() + len_; }
    bool operator==(StringView other) const {
        if (len_ != other.len_) return false;
        if (len_ == 0) return true;
        return std::memcmp(data_, other.data_, len_) == 0;
    }
private:
    const char* data_ = nullptr;
    size_t len_ = 0;
};

class String {
public:
    static constexpr size_t kSmallCapacity = 23;

    String() { initSmall(); }
    String(const char* s) : String(s, s ? std::strlen(s) : 0) {}
    String(const std::string& s) : String(s.data(), s.size()) {}
    String(std::string_view s) : String(s.data(), s.size()) {}
    String(StringView s) : String(s.data(), s.length()) {}
    String(const char* s, size_t len) {
        initSmall();
        assign(s, len);
    }
    explicit String(char c) : String(&c, 1) {}
    String(const String& o) {
        initSmall();
        assign(o.data_, o.len_);
    }
    String(String&& o) noexcept {
        initSmall();
        moveAssign(o);
    }
    ~String() { releaseHeap(); }
    String& operator=(const String& o) {
        if (this != &o) {
            assign(o.data_, o.len_);
        }
        return *this;
    }
    String& operator=(std::string_view s) {
        assign(s.data(), s.size());
        return *this;
    }
    String& operator=(String&& o) noexcept {
        if (this != &o) {
            moveAssign(o);
        }
        return *this;
    }

    String& operator+=(const String& o) { return append(o.data_, o.len_); }
    String& operator+=(const char* s) { return append(s, s?std::strlen(s):0); }
    String& operator+=(const std::string& s) { return append(s.data(), s.size()); }
    String& operator+=(std::string_view s) { return append(s.data(), s.size()); }
    String& operator+=(StringView s) { return append(s.data(), s.length()); }
    String& operator+=(char c) { return append(&c, 1); }
    String& reserve(size_t capacity) {
        if (capacity > cap_) {
            grow(capacity);
        }
        return *this;
    }
    void clear() {
        len_ = 0;
        data_[0] = '\0';
    }
    String& append(const char* s, size_t len) {
        if (!s || !len) {
            return *this;
        }

        const bool alias = s >= data_ && s < data_ + len_;
        const size_t aliasOffset = alias ? static_cast<size_t>(s - data_) : 0;
        const size_t nl = len_ + len;
        if (nl > cap_) {
            grow(nl);
        }
        if (alias) {
            s = data_ + aliasOffset;
        }
        std::memmove(data_ + len_, s, len);
        len_ = nl;
        data_[len_] = '\0';
        return *this;
    }
    String& append(std::string_view s) {
        return append(s.data(), s.size());
    }
    String& append(StringView s) {
        return append(s.data(), s.length());
    }
    String& append(const std::string& s) {
        return append(s.data(), s.size());
    }

    StringView mid(size_t pos, size_t count=~0ULL) const {
        if(pos>=len_)return StringView();
        if(count>len_-pos)count=len_-pos;
        return StringView(data_+pos,count);
    }
    String substr(size_t pos, size_t count=~0ULL) const {
        if (pos >= len_) {
            return String();
        }
        if (count > len_ - pos) {
            count = len_ - pos;
        }
        return String(data_ + pos, count);
    }
    StringView left(size_t n) const { return mid(0,n); }
    StringView right(size_t n) const { return n>=len_?StringView(data_,len_):mid(len_-n,n); }

    int indexOf(char c, size_t from=0) const {
        for(size_t i=from;i<len_;++i)if(data_[i]==c)return(int)i;
        return -1;
    }
    int indexOf(const char* s, size_t from=0) const {
        size_t sl=std::strlen(s);
        if(!sl||sl>len_)return -1;
        for(size_t i=from;i<=len_-sl;++i)if(std::memcmp(data_+i,s,sl)==0)return(int)i;
        return -1;
    }
    int indexOf(std::string_view s, size_t from=0) const {
        if (s.empty() || s.size() > len_) {
            return -1;
        }
        for (size_t i = from; i <= len_ - s.size(); ++i) {
            if (std::memcmp(data_ + i, s.data(), s.size()) == 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    size_t find(const char* s, size_t from=0) const {
        const int idx = indexOf(s, from);
        return idx >= 0 ? static_cast<size_t>(idx) : static_cast<size_t>(-1);
    }
    size_t find(char c, size_t from=0) const {
        const int idx = indexOf(c, from);
        return idx >= 0 ? static_cast<size_t>(idx) : static_cast<size_t>(-1);
    }
    size_t find(std::string_view s, size_t from=0) const {
        const int idx = indexOf(s, from);
        return idx >= 0 ? static_cast<size_t>(idx) : static_cast<size_t>(-1);
    }
    int lastIndexOf(char c) const {
        for(size_t i=len_;i>0;--i)if(data_[i-1]==c)return(int)(i-1);
        return -1;
    }
    bool contains(char c) const { return indexOf(c)>=0; }
    bool contains(const char* s) const { return indexOf(s)>=0; }
    bool contains(std::string_view s) const { return indexOf(s)>=0; }
    bool contains(StringView s) const { return indexOf(s.data() ? std::string_view(s.data(), s.length()) : std::string_view())>=0; }
    bool startsWith(const char* s) const { size_t sl=std::strlen(s); return sl<=len_&&std::memcmp(data_,s,sl)==0; }
    bool startsWith(std::string_view s) const { return s.size()<=len_&&std::memcmp(data_,s.data(),s.size())==0; }
    bool startsWith(StringView s) const { return s.length()<=len_&&std::memcmp(data_,s.data(),s.length())==0; }
    bool endsWith(const char* s) const { size_t sl=std::strlen(s); return sl<=len_&&std::memcmp(data_+len_-sl,s,sl)==0; }
    bool endsWith(std::string_view s) const { return s.size()<=len_&&std::memcmp(data_+len_-s.size(),s.data(),s.size())==0; }
    bool endsWith(StringView s) const { return s.length()<=len_&&std::memcmp(data_+len_-s.length(),s.data(),s.length())==0; }
    int count(char c) const { int n=0; for(size_t i=0;i<len_;++i)if(data_[i]==c)++n; return n; }
    String& replace(size_t pos, size_t count, const char* s) {
        if (pos > len_) {
            return *this;
        }
        const size_t replLen = s ? std::strlen(s) : 0;
        if (count > len_ - pos) {
            count = len_ - pos;
        }
        const size_t tailLen = len_ - pos - count;
        String out;
        out.reserve(len_ - count + replLen);
        out.append(data_, pos);
        out.append(s, replLen);
        out.append(data_ + pos + count, tailLen);
        swap(out);
        return *this;
    }
    String& replace(size_t pos, size_t count, std::string_view s) {
        if (pos > len_) {
            return *this;
        }
        if (count > len_ - pos) {
            count = len_ - pos;
        }
        const size_t tailLen = len_ - pos - count;
        String out;
        out.reserve(len_ - count + s.size());
        out.append(data_, pos);
        out.append(s);
        out.append(data_ + pos + count, tailLen);
        swap(out);
        return *this;
    }
    String& replace(size_t pos, size_t count, const std::string& s) {
        return replace(pos, count, std::string_view(s));
    }

    int toInt(bool* ok=nullptr) const {
        if(!data_){if(ok)*ok=false;return 0;}
        char* end=nullptr; long v=std::strtol(data_,&end,10);
        if(ok)*ok=(end!=data_); return(int)v;
    }
    double toDouble(bool* ok=nullptr) const {
        if(!data_){if(ok)*ok=false;return 0.0;}
        char* end=nullptr; double v=std::strtod(data_,&end);
        if(ok)*ok=(end!=data_); return v;
    }

    void swap(String& o) {
        if (this == &o) {
            return;
        }
        String tmp(std::move(o));
        o = std::move(*this);
        *this = std::move(tmp);
    }
    static String number(int n) { char b[32]; std::snprintf(b,sizeof(b),"%d",n); return String(b); }
    static String number(double d, int dec=2) { char b[64]; if(dec<0)dec=0; std::snprintf(b,sizeof(b),"%.*f",dec,d); return String(b); }

    size_t length() const { return len_; }
    bool isEmpty() const { return len_==0; }
    size_t capacity() const { return cap_; }
    const char* data() const { return data_; }
    operator std::string_view() const { return std::string_view(data_, len_); }
    char at(size_t i) const { return (i<len_)?data_[i]:0; }
    char operator[](size_t i) const { return at(i); }

    bool operator==(const String& o) const { return len_==o.len_&&(len_==0||std::memcmp(data_,o.data_,len_)==0); }
    bool operator==(const char* s) const { size_t sl=s?std::strlen(s):0; return sl==len_&&(sl==0||std::memcmp(data_,s,sl)==0); }
    bool operator!=(const String& o) const { return !(*this==o); }
    bool operator!=(const char* s) const { return !(*this==s); }
    bool operator<(const String& o) const {
        size_t ml=len_<o.len_?len_:o.len_;
        int c=ml>0?std::memcmp(data_,o.data_,ml):0;
        return c<0||(c==0&&len_<o.len_);
    }

private:
    void initSmall() {
        data_ = small_;
        len_ = 0;
        cap_ = kSmallCapacity;
        small_[0] = '\0';
    }

    bool isSmall() const {
        return data_ == small_;
    }

    void releaseHeap() {
        if (!isSmall()) {
            ::operator delete(data_);
        }
        data_ = small_;
        len_ = 0;
        cap_ = kSmallCapacity;
        small_[0] = '\0';
    }

    void moveAssign(String& o) {
        if (o.isSmall()) {
            releaseHeap();
            assign(o.data_, o.len_);
            o.clear();
            return;
        }

        releaseHeap();
        data_ = o.data_;
        len_ = o.len_;
        cap_ = o.cap_;
        o.initSmall();
    }

    void grow(size_t minCapacity) {
        size_t newCapacity = cap_ ? cap_ * 2 : kSmallCapacity;
        if (newCapacity < minCapacity) {
            newCapacity = minCapacity;
        }

        char* newData = static_cast<char*>(::operator new(newCapacity + 1));
        std::memmove(newData, data_, len_);
        newData[len_] = '\0';

        if (!isSmall()) {
            ::operator delete(data_);
        }

        data_ = newData;
        cap_ = newCapacity;
    }

    void assign(const char* s, size_t len) {
        if (!s || !len) {
            clear();
            return;
        }

        if (len > cap_) {
            grow(len);
        }

        std::memmove(data_, s, len);
        len_ = len;
        data_[len_] = '\0';
    }

    char small_[kSmallCapacity + 1] = {};
    char* data_ = small_;
    size_t len_ = 0;
    size_t cap_ = kSmallCapacity;
};

inline String operator+(const String& a, const String& b) { String r(a); r+=b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r+=b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r+=b; return r; }
inline String operator+(const String& a, std::string_view b) { String r(a); r+=b; return r; }
inline String operator+(std::string_view a, const String& b) { String r(a); r+=b; return r; }

inline std::string toStdString(const String& s) {
    return std::string(s.data(), s.length());
}

inline std::string toStdString(StringView s) {
    return std::string(s.data(), s.length());
}

inline std::string toStdString(std::string_view s) {
    return std::string(s.data(), s.size());
}

inline std::string toStdString(const char* s) {
    return s ? std::string(s) : std::string();
}

using ZeroString = String;

} // namespace ArtifactCore
