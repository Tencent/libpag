#pragma once

#include <glbinding/glbinding_api.h>

#include <ostream>
#include <string>
#include <set>
#include <utility>


namespace glbinding
{

class GLBINDING_API Version
{
public:
    Version();
    Version(int majorVersion, int minorVersion);

    bool operator< (const Version & version) const;
    bool operator> (const Version & version) const;
    bool operator==(const Version & version) const;
    bool operator!=(const Version & version) const;
    bool operator>=(const Version & version) const;
    bool operator<=(const Version & version) const;

    operator std::pair<unsigned char,  unsigned char> () const;
    operator std::pair<unsigned short, unsigned short>() const;
    operator std::pair<unsigned int,   unsigned int>  () const;

    std::string toString() const;

    bool isValid() const;
    bool isNull() const;

    const Version & nearest() const;

    static const std::set<Version> & versions();
    static const Version & latest();

public:
    int m_major;
    int m_minor;

protected:
    static const std::set<Version> s_validVersions;
    static const Version s_latest;
};

} // namespace glbinding

GLBINDING_API std::ostream & operator<<(std::ostream & stream, const glbinding::Version & version);
