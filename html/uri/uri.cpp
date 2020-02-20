#include "uri.h"
#include <cctype>
#include <stdexcept>
namespace web
{

template <class S>
S& to_lower_in_place(S& str)
/// Replaces all characters in str with their lower-case counterparts.
{
    auto it = str.begin();
    auto end = str.end();

    while(it != end)
    {
        *it = static_cast<typename S::value_type>(std::tolower(*it));
        ++it;
    }
    return str;
}

const std::string reserved_path{"?#"};
const std::string reserved_query{"?#/:;+@"};
const std::string reserved_query_param{"?#/:;+@&="};
const std::string reserved_fragment{};
const std::string illegal{"%<>{}|\\\"^`!*'()$,[]"};

URI::URI(const std::string& uri)
{
    parse(uri);
}

URI::URI(const std::string& scheme, const std::string& pathEtc)
    : scheme_(scheme)
    , port_(0)
{
    to_lower_in_place(scheme_);
    port_ = get_well_known_port();
    std::string::const_iterator beg = pathEtc.begin();
    std::string::const_iterator end = pathEtc.end();
    parse_path_etc(beg, end);
}

URI::URI(const std::string& scheme, const std::string& authority, const std::string& pathEtc)
    : scheme_(scheme)
{
    to_lower_in_place(scheme_);
    auto beg = authority.cbegin();
    auto end = authority.cend();
    parse_authority(beg, end);
    beg = pathEtc.begin();
    end = pathEtc.end();
    parse_path_etc(beg, end);
}

URI::URI(const std::string& scheme, const std::string& authority, const std::string& path,
         const std::string& query)
    : scheme_(scheme)
    , path_(path)
    , query_(query)
{
    to_lower_in_place(scheme_);
    auto beg = authority.cbegin();
    auto end = authority.cend();
    parse_authority(beg, end);
}

URI::URI(const std::string& scheme, const std::string& authority, const std::string& path,
         const std::string& query, const std::string& fragment)
    : scheme_(scheme)
    , path_(path)
    , query_(query)
    , fragment_(fragment)
{
    to_lower_in_place(scheme_);
    auto beg = authority.cbegin();
    auto end = authority.cend();
    parse_authority(beg, end);
}

URI::URI(const URI& baseURI, const std::string& relativeURI)
    : scheme_(baseURI.scheme_)
    , user_info_(baseURI.user_info_)
    , host_(baseURI.host_)
    , port_(baseURI.port_)
    , path_(baseURI.path_)
    , query_(baseURI.query_)
    , fragment_(baseURI.fragment_)
{
    resolve(relativeURI);
}

URI& URI::operator=(const std::string& uri)
{
    clear();
    parse(uri);
    return *this;
}

void URI::swap(URI& uri)
{
    std::swap(scheme_, uri.scheme_);
    std::swap(user_info_, uri.user_info_);
    std::swap(host_, uri.host_);
    std::swap(port_, uri.port_);
    std::swap(path_, uri.path_);
    std::swap(query_, uri.query_);
    std::swap(fragment_, uri.fragment_);
}

void URI::clear()
{
    scheme_.clear();
    user_info_.clear();
    host_.clear();
    port_ = 0;
    path_.clear();
    query_.clear();
    fragment_.clear();
}

std::string URI::to_string() const
{
    std::string uri;
    if(is_relative())
    {
        encode(path_, reserved_path, uri);
    }
    else
    {
        uri = scheme_;
        uri += ':';
        std::string auth = get_authority();
        if(!auth.empty() || scheme_ == "file")
        {
            uri.append("//");
            uri.append(auth);
        }
        if(!path_.empty())
        {
            if(!auth.empty() && path_[0] != '/')
                uri += '/';
            encode(path_, reserved_path, uri);
        }
        else if(!query_.empty() || !fragment_.empty())
        {
            uri += '/';
        }
    }
    if(!query_.empty())
    {
        uri += '?';
        uri.append(query_);
    }
    if(!fragment_.empty())
    {
        uri += '#';
        encode(fragment_, reserved_fragment, uri);
    }
    return uri;
}

void URI::set_scheme(const std::string& scheme)
{
    scheme_ = scheme;
    to_lower_in_place(scheme_);
    if(port_ == 0)
        port_ = get_well_known_port();
}

void URI::set_user_info(const std::string& userInfo)
{
    user_info_.clear();
    decode(userInfo, user_info_);
}

void URI::set_host(const std::string& host)
{
    host_ = host;
}

unsigned short URI::get_port() const
{
    if(port_ == 0)
        return get_well_known_port();

    return port_;
}

void URI::set_port(unsigned short port)
{
    port_ = port;
}

std::string URI::get_authority() const
{
    std::string auth;
    if(!user_info_.empty())
    {
        auth.append(user_info_);
        auth += '@';
    }
    if(host_.find(':') != std::string::npos)
    {
        auth += '[';
        auth += host_;
        auth += ']';
    }
    else
        auth.append(host_);
    if(port_ && !is_well_known_port())
    {
        auth += ':';
        // NumberFormatter::append(auth, _port);
        auth += std::to_string(port_);
    }
    return auth;
}

void URI::set_authority(const std::string& authority)
{
    user_info_.clear();
    host_.clear();
    port_ = 0;
    auto beg = authority.cbegin();
    auto end = authority.cend();
    parse_authority(beg, end);
}

void URI::set_path(const std::string& path)
{
    path_.clear();
    decode(path, path_);
}

void URI::set_raw_query(const std::string& query)
{
    query_ = query;
}

void URI::set_query(const std::string& query)
{
    query_.clear();
    encode(query, reserved_query, query_);
}

void URI::add_query_parameter(const std::string& param, const std::string& val)
{
    if(!query_.empty())
        query_ += '&';
    encode(param, reserved_query_param, query_);
    query_ += '=';
    encode(val, reserved_query_param, query_);
}

std::string URI::get_query() const
{
    std::string query;
    decode(query_, query);
    return query;
}

URI::query_parameters URI::get_query_parameters() const
{
    query_parameters result;
    auto it = query_.cbegin();
    auto end = query_.cend();
    while(it != end)
    {
        std::string name;
        std::string value;
        while(it != end && *it != '=' && *it != '&')
        {
            if(*it == '+')
                name += ' ';
            else
                name += *it;
            ++it;
        }
        if(it != end && *it == '=')
        {
            ++it;
            while(it != end && *it != '&')
            {
                if(*it == '+')
                    value += ' ';
                else
                    value += *it;
                ++it;
            }
        }
        std::string decodedName;
        std::string decodedValue;
        URI::decode(name, decodedName);
        URI::decode(value, decodedValue);
        result.push_back(std::make_pair(decodedName, decodedValue));
        if(it != end && *it == '&')
            ++it;
    }
    return result;
}

void URI::set_query_parameters(const query_parameters& params)
{
    query_.clear();
    for(const auto& param : params)
    {
        add_query_parameter(param.first, param.second);
    }
}

void URI::set_fragment(const std::string& fragment)
{
    fragment_.clear();
    decode(fragment, fragment_);
}

void URI::set_path_etc(const std::string& pathEtc)
{
    path_.clear();
    query_.clear();
    fragment_.clear();
    std::string::const_iterator beg = pathEtc.begin();
    std::string::const_iterator end = pathEtc.end();
    parse_path_etc(beg, end);
}

std::string URI::get_path_etc() const
{
    std::string pathEtc;
    encode(path_, reserved_path, pathEtc);
    if(!query_.empty())
    {
        pathEtc += '?';
        pathEtc += query_;
    }
    if(!fragment_.empty())
    {
        pathEtc += '#';
        encode(fragment_, reserved_fragment, pathEtc);
    }
    return pathEtc;
}

std::string URI::get_path_and_query() const
{
    std::string pathAndQuery;
    encode(path_, reserved_path, pathAndQuery);
    if(!query_.empty())
    {
        pathAndQuery += '?';
        pathAndQuery += query_;
    }
    return pathAndQuery;
}

void URI::resolve(const std::string& relativeURI)
{
    URI parsedURI(relativeURI);
    resolve(parsedURI);
}

void URI::resolve(const URI& relativeURI)
{
    if(!relativeURI.scheme_.empty())
    {
        scheme_ = relativeURI.scheme_;
        user_info_ = relativeURI.user_info_;
        host_ = relativeURI.host_;
        port_ = relativeURI.port_;
        path_ = relativeURI.path_;
        query_ = relativeURI.query_;
        remove_dot_segments();
    }
    else
    {
        if(!relativeURI.host_.empty())
        {
            user_info_ = relativeURI.user_info_;
            host_ = relativeURI.host_;
            port_ = relativeURI.port_;
            path_ = relativeURI.path_;
            query_ = relativeURI.query_;
            remove_dot_segments();
        }
        else
        {
            if(relativeURI.path_.empty())
            {
                if(!relativeURI.query_.empty())
                    query_ = relativeURI.query_;
            }
            else
            {
                if(relativeURI.path_[0] == '/')
                {
                    path_ = relativeURI.path_;
                    remove_dot_segments();
                }
                else
                {
                    merge_path(relativeURI.path_);
                }
                query_ = relativeURI.query_;
            }
        }
    }
    fragment_ = relativeURI.fragment_;
}

bool URI::is_relative() const
{
    return scheme_.empty();
}

bool URI::empty() const
{
    return scheme_.empty() && host_.empty() && path_.empty() && query_.empty() && fragment_.empty();
}

bool URI::operator==(const URI& uri) const
{
    return equals(uri);
}

bool URI::operator==(const std::string& uri) const
{
    URI parsedURI(uri);
    return equals(parsedURI);
}

bool URI::operator!=(const URI& uri) const
{
    return !equals(uri);
}

bool URI::operator!=(const std::string& uri) const
{
    URI parsedURI(uri);
    return !equals(parsedURI);
}

bool URI::equals(const URI& uri) const
{
    return scheme_ == uri.scheme_ && user_info_ == uri.user_info_ && host_ == uri.host_ &&
           get_port() == uri.get_port() && path_ == uri.path_ && query_ == uri.query_ &&
           fragment_ == uri.fragment_;
}

void URI::normalize()
{
    remove_dot_segments(!is_relative());
}

void URI::remove_dot_segments(bool removeLeading)
{
    if(path_.empty())
        return;

    bool leadingSlash = *(path_.begin()) == '/';
    bool trailingSlash = *(path_.rbegin()) == '/';
    std::vector<std::string> segments;
    std::vector<std::string> normalizedSegments;
    get_path_segments(segments);
    for(std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); ++it)
    {
        if(*it == "..")
        {
            if(!normalizedSegments.empty())
            {
                if(normalizedSegments.back() == "..")
                    normalizedSegments.push_back(*it);
                else
                    normalizedSegments.pop_back();
            }
            else if(!removeLeading)
            {
                normalizedSegments.push_back(*it);
            }
        }
        else if(*it != ".")
        {
            normalizedSegments.push_back(*it);
        }
    }
    build_path(normalizedSegments, leadingSlash, trailingSlash);
}

void URI::get_path_segments(std::vector<std::string>& segments)
{
    get_path_segments(path_, segments);
}

void URI::get_path_segments(const std::string& path, std::vector<std::string>& segments)
{
    std::string::const_iterator it = path.begin();
    std::string::const_iterator end = path.end();
    std::string seg;
    while(it != end)
    {
        if(*it == '/')
        {
            if(!seg.empty())
            {
                segments.push_back(seg);
                seg.clear();
            }
        }
        else
            seg += *it;
        ++it;
    }
    if(!seg.empty())
        segments.push_back(seg);
}

void URI::encode(const std::string& str, const std::string& reserved, std::string& encodedStr)
{
    for(char c : str)
    {
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '_' || c == '.' || c == '~')
        {
            encodedStr += c;
        }
        else if(c <= 0x20 || c >= 0x7F || illegal.find(c) != std::string::npos ||
                reserved.find(c) != std::string::npos)
        {
            encodedStr += '%';
            // encodedStr += NumberFormatter::formatHex((unsigned) (unsigned char) c, 2);
        }
        else
            encodedStr += c;
    }
}

void URI::decode(const std::string& str, std::string& decodedStr, bool plusAsSpace)
{
    bool inQuery = false;
    auto it = str.cbegin();
    auto end = str.cend();
    while(it != end)
    {
        char c = *it++;
        if(c == '?')
            inQuery = true;
        // spaces may be encoded as plus signs in the query
        if(inQuery && plusAsSpace && c == '+')
            c = ' ';
        else if(c == '%')
        {
            if(it == end)
                throw std::runtime_error("URI encoding: no hex digit following percent sign " + str);
            char hi = *it++;
            if(it == end)
                throw std::runtime_error("URI encoding: two hex digits must follow percent sign " + str);
            char lo = *it++;
            if(hi >= '0' && hi <= '9')
                c = hi - '0';
            else if(hi >= 'A' && hi <= 'F')
                c = hi - 'A' + 10;
            else if(hi >= 'a' && hi <= 'f')
                c = hi - 'a' + 10;
            else
                throw std::runtime_error("URI encoding: not a hex digit");
            c *= 16;
            if(lo >= '0' && lo <= '9')
                c += lo - '0';
            else if(lo >= 'A' && lo <= 'F')
                c += lo - 'A' + 10;
            else if(lo >= 'a' && lo <= 'f')
                c += lo - 'a' + 10;
            else
                throw std::runtime_error("URI encoding: not a hex digit");
        }
        decodedStr += c;
    }
}

bool URI::is_well_known_port() const
{
    return port_ == get_well_known_port();
}

unsigned short URI::get_well_known_port() const
{
    if(scheme_ == "ftp")
        return 21;
    if(scheme_ == "ssh")
        return 22;
    if(scheme_ == "telnet")
        return 23;
    if(scheme_ == "smtp")
        return 25;
    if(scheme_ == "dns")
        return 53;
    if(scheme_ == "http" || scheme_ == "ws")
        return 80;
    if(scheme_ == "nntp")
        return 119;
    if(scheme_ == "imap")
        return 143;
    if(scheme_ == "ldap")
        return 389;
    if(scheme_ == "https" || scheme_ == "wss")
        return 443;
    if(scheme_ == "smtps")
        return 465;
    if(scheme_ == "rtsp")
        return 554;
    if(scheme_ == "ldaps")
        return 636;
    if(scheme_ == "dnss")
        return 853;
    if(scheme_ == "imaps")
        return 993;
    if(scheme_ == "sip")
        return 5060;
    if(scheme_ == "sips")
        return 5061;
    if(scheme_ == "xmpp")
        return 5222;

    return 0;
}

void URI::parse(const std::string& uri)
{
    auto it = uri.cbegin();
    auto end = uri.cend();
    if(it == end)
        return;
    if(*it != '/' && *it != '.' && *it != '?' && *it != '#')
    {
        std::string scheme;
        while(it != end && *it != ':' && *it != '?' && *it != '#' && *it != '/')
            scheme += *it++;
        if(it != end && *it == ':')
        {
            ++it;
            if(it == end)
                throw std::runtime_error("URI scheme must be followed by authority or path " + uri);
            set_scheme(scheme);
            if(*it == '/')
            {
                ++it;
                if(it != end && *it == '/')
                {
                    ++it;
                    parse_authority(it, end);
                }
                else
                    --it;
            }
            parse_path_etc(it, end);
        }
        else
        {
            it = uri.begin();
            parse_path_etc(it, end);
        }
    }
    else
        parse_path_etc(it, end);
}

void URI::parse_authority(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    std::string userInfo;
    std::string part;
    while(it != end && *it != '/' && *it != '?' && *it != '#')
    {
        if(*it == '@')
        {
            userInfo = part;
            part.clear();
        }
        else
            part += *it;
        ++it;
    }
    auto pbeg = part.cbegin();
    auto pend = part.cend();
    parse_host_and_port(pbeg, pend);
    user_info_ = userInfo;
}

void URI::parse_host_and_port(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    if(it == end)
        return;
    std::string host;
    if(*it == '[')
    {
        // IPv6 address
        ++it;
        while(it != end && *it != ']')
            host += *it++;
        if(it == end)
            throw std::runtime_error("unterminated IPv6 address");
        ++it;
    }
    else
    {
        while(it != end && *it != ':')
            host += *it++;
    }
    if(it != end && *it == ':')
    {
        ++it;
        std::string port;
        while(it != end)
            port += *it++;
        if(!port.empty())
        {
            int nport = std::stoi(port);
            if(nport > 0 && nport < 65536)
                port_ = (unsigned short)nport;
            else
                throw std::runtime_error("bad or invalid port number " + port);
        }
        else
            port_ = get_well_known_port();
    }
    else
        port_ = get_well_known_port();
    host_ = host;
    to_lower_in_place(host_);
}

void URI::parse_path(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    std::string path;
    while(it != end && *it != '?' && *it != '#')
        path += *it++;
    decode(path, path_);
}

void URI::parse_path_etc(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    if(it == end)
        return;
    if(*it != '?' && *it != '#')
        parse_path(it, end);
    if(it != end && *it == '?')
    {
        ++it;
        parse_query(it, end);
    }
    if(it != end && *it == '#')
    {
        ++it;
        parse_fragment(it, end);
    }
}

void URI::parse_query(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    query_.clear();
    while(it != end && *it != '#')
        query_ += *it++;
}

void URI::parse_fragment(std::string::const_iterator& it, const std::string::const_iterator& end)
{
    std::string fragment;
    while(it != end)
        fragment += *it++;
    decode(fragment, fragment_);
}

void URI::merge_path(const std::string& path)
{
    std::vector<std::string> segments;
    std::vector<std::string> normalizedSegments;
    bool addLeadingSlash = false;
    if(!path_.empty())
    {
        get_path_segments(segments);
        bool endsWithSlash = *(path_.rbegin()) == '/';
        if(!endsWithSlash && !segments.empty())
            segments.pop_back();
        addLeadingSlash = path_[0] == '/';
    }
    get_path_segments(path, segments);
    addLeadingSlash = addLeadingSlash || (!path.empty() && path[0] == '/');
    bool hasTrailingSlash = (!path.empty() && *(path.rbegin()) == '/');
    bool addTrailingSlash = false;
    for(std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); ++it)
    {
        if(*it == "..")
        {
            addTrailingSlash = true;
            if(!normalizedSegments.empty())
                normalizedSegments.pop_back();
        }
        else if(*it != ".")
        {
            addTrailingSlash = false;
            normalizedSegments.push_back(*it);
        }
        else
            addTrailingSlash = true;
    }
    build_path(normalizedSegments, addLeadingSlash, hasTrailingSlash || addTrailingSlash);
}

void URI::build_path(const std::vector<std::string>& segments, bool leadingSlash, bool trailingSlash)
{
    path_.clear();
    bool first = true;
    for(std::vector<std::string>::const_iterator it = segments.begin(); it != segments.end(); ++it)
    {
        if(first)
        {
            first = false;
            if(leadingSlash)
                path_ += '/';
            else if(scheme_.empty() && (*it).find(':') != std::string::npos)
                path_.append("./");
        }
        else
            path_ += '/';
        path_.append(*it);
    }
    if(trailingSlash)
        path_ += '/';
}

} // namespace
