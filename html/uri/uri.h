#ifndef URI_H_INCLUDED
#define URI_H_INCLUDED

#include <string>
#include <utility>
#include <vector>

namespace web
{

/// A Uniform Resource Identifier, as specified in RFC 3986.
///
/// The URI class provides methods for building URIs from their
/// parts, as well as for splitting URIs into their parts.
/// Furthermore, the class provides methods for resolving
/// relative URIs against base URIs.
///
/// The class automatically performs a few normalizations on
/// all URIs and URI parts passed to it:
///   * scheme identifiers are converted to lower case
///   * percent-encoded characters are decoded (except for the query string)
///   * optionally, dot segments are removed from paths (see normalize())
///
/// Note that dealing with query strings requires some precautions, as, internally,
/// query strings are stored in percent-encoded form, while all other parts of the URI
/// are stored in decoded form. While parsing query strings from properly encoded URLs
/// generally works, explicitly setting query strings with set_query() or extracting
/// query strings with get_query() may lead to ambiguities. See the descriptions of
/// set_query(), set_raw_query(), get_query() and get_raw_query() for more information.
class URI
{
public:
    using query_parameters = std::vector<std::pair<std::string, std::string>>;

    /// Creates an empty URI.
    URI() = default;

    /// Parses an URI from the given string. Throws a
    /// SyntaxException if the uri is not valid.
    explicit URI(const std::string& uri);

    /// Creates an URI from its parts.
    URI(const std::string& scheme, const std::string& pathEtc);

    /// Creates an URI from its parts.
    URI(const std::string& scheme, const std::string& authority, const std::string& pathEtc);

    /// Creates an URI from its parts.
    URI(const std::string& scheme, const std::string& authority, const std::string& path,
        const std::string& query);

    /// Creates an URI from its parts.
    URI(const std::string& scheme, const std::string& authority, const std::string& path,
        const std::string& query, const std::string& fragment);

    /// Creates an URI from a base URI and a relative URI, according to
    /// the algorithm in section 5.2 of RFC 3986.
    URI(const URI& baseURI, const std::string& relativeURI);

    /// Parses and assigns an URI from the given string. Throws a
    /// SyntaxException if the uri is not valid.
    URI& operator=(const std::string& uri);

    /// Swaps the URI with another one.
    void swap(URI& uri);

    /// Clears all parts of the URI.
    void clear();

    /// Returns a string representation of the URI.
    ///
    /// Characters in the path, query and fragment parts will be
    /// percent-encoded as necessary.
    std::string to_string() const;

    /// Returns the scheme part of the URI.
    const std::string& get_scheme() const;

    /// Sets the scheme part of the URI. The given scheme
    /// is converted to lower-case.
    ///
    /// A list of registered URI schemes can be found
    /// at <http://www.iana.org/assignments/uri-schemes>.
    void set_scheme(const std::string& scheme);

    /// Returns the user-info part of the URI.
    const std::string& get_user_info() const;

    /// Sets the user-info part of the URI.
    void set_user_info(const std::string& userInfo);

    /// Returns the host part of the URI.
    const std::string& get_host() const;

    /// Sets the host part of the URI.
    void set_host(const std::string& host);

    /// Returns the port number part of the URI.
    ///
    /// If no port number (0) has been specified, the
    /// well-known port number (e.g., 80 for http) for
    /// the given scheme is returned if it is known.
    /// Otherwise, 0 is returned.
    unsigned short get_port() const;

    /// Sets the port number part of the URI.
    void set_port(unsigned short port);

    /// Returns the authority part (userInfo, host and port)
    /// of the URI.
    ///
    /// If the port number is a well-known port
    /// number for the given scheme (e.g., 80 for http), it
    /// is not included in the authority.
    std::string get_authority() const;

    /// Parses the given authority part for the URI and sets
    /// the user-info, host, port components accordingly.
    void set_authority(const std::string& authority);

    /// Returns the decoded path part of the URI.
    const std::string& get_path() const;

    /// Sets the path part of the URI.
    void set_path(const std::string& path);

    /// Returns the decoded query part of the URI.
    ///
    /// Note that encoded ampersand characters ('&', "%26")
    /// will be decoded, which could cause ambiguities if the query
    /// string contains multiple parameters and a parameter name
    /// or value contains an ampersand as well.
    /// In such a case it's better to use getRawQuery() or
    /// getQueryParameters().
    std::string get_query() const;

    /// Sets the query part of the URI.
    ///
    /// The query string will be percent-encoded. If the query
    /// already contains percent-encoded characters, these
    /// will be double-encoded, which is probably not what's
    /// intended by the caller. Furthermore, ampersand ('&')
    /// characters in the query will not be encoded. This could
    /// lead to ambiguity issues if the query string contains multiple
    /// name-value parameters separated by ampersand, and if any
    /// name or value also contains an ampersand. In such a
    /// case, it's better to use setRawQuery() with a properly
    /// percent-encoded query string, or use addQueryParameter()
    /// or setQueryParameters(), which take care of appropriate
    /// percent encoding of parameter names and values.
    void set_query(const std::string& query);

    /// Adds "param=val" to the query; "param" may not be empty.
    /// If val is empty, only '=' is appended to the parameter.
    ///
    /// In addition to regular encoding, function also encodes '&' and '=',
    /// if found in param or val.
    void add_query_parameter(const std::string& param, const std::string& val = "");

    /// Returns the query string in raw form, which usually
    /// means percent encoded.
    const std::string& get_raw_query() const;

    /// Sets the query part of the URI.
    ///
    /// The given query string must be properly percent-encoded.
    void set_raw_query(const std::string& query);

    /// Returns the decoded query string parameters as a vector
    /// of name-value pairs.
    query_parameters get_query_parameters() const;

    /// Sets the query part of the URI from a vector
    /// of query parameters.
    ///
    /// Calls addQueryParameter() for each parameter name and value.
    void set_query_parameters(const query_parameters& params);

    /// Returns the fragment part of the URI.
    const std::string& get_fragment() const;

    /// Sets the fragment part of the URI.
    void set_fragment(const std::string& fragment);

    /// Sets the path, query and fragment parts of the URI.
    void set_path_etc(const std::string& pathEtc);

    /// Returns the encoded path, query and fragment parts of the URI.
    std::string get_path_etc() const;

    /// Returns the encoded path and query parts of the URI.
    std::string get_path_and_query() const;

    /// Resolves the given relative URI against the base URI.
    /// See section 5.2 of RFC 3986 for the algorithm used.
    void resolve(const std::string& relativeURI);

    /// Resolves the given relative URI against the base URI.
    /// See section 5.2 of RFC 3986 for the algorithm used.
    void resolve(const URI& relativeURI);

    /// Returns true if the URI is a relative reference, false otherwise.
    ///
    /// A relative reference does not contain a scheme identifier.
    /// Relative references are usually resolved against an absolute
    /// base reference.
    bool is_relative() const;

    /// Returns true if the URI is empty, false otherwise.
    bool empty() const;

    /// Returns true if both URIs are identical, false otherwise.
    ///
    /// Two URIs are identical if their scheme, authority,
    /// path, query and fragment part are identical.
    bool operator==(const URI& uri) const;

    /// Parses the given URI and returns true if both URIs are identical,
    /// false otherwise.
    bool operator==(const std::string& uri) const;

    /// Returns true if both URIs are identical, false otherwise.
    bool operator!=(const URI& uri) const;

    /// Parses the given URI and returns true if both URIs are identical,
    /// false otherwise.
    bool operator!=(const std::string& uri) const;

    /// Normalizes the URI by removing all but leading . and .. segments from the path.
    ///
    /// If the first path segment in a relative path contains a colon (:),
    /// such as in a Windows path containing a drive letter, a dot segment (./)
    /// is prepended in accordance with section 3.3 of RFC 3986.
    void normalize();

    /// Places the single path segments (delimited by slashes) into the
    /// given vector.
    void get_path_segments(std::vector<std::string>& segments);

    /// URI-encodes the given string by escaping reserved and non-ASCII
    /// characters. The encoded string is appended to encodedStr.
    static void encode(const std::string& str, const std::string& reserved, std::string& encodedStr);

    /// URI-decodes the given string by replacing percent-encoded
    /// characters with the actual character. The decoded string
    /// is appended to decodedStr.
    ///
    /// When plusAsSpace is true, non-encoded plus signs in the query are decoded as spaces.
    /// (http://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.1)
    static void decode(const std::string& str, std::string& decodedStr, bool plusAsSpace = false);

protected:
    /// Returns true if both uri's are equivalent.
    bool equals(const URI& uri) const;

    /// Returns true if the URI's port number is a well-known one
    /// (for example, 80, if the scheme is http).
    bool is_well_known_port() const;

    /// Returns the well-known port number for the URI's scheme,
    /// or 0 if the port number is not known.
    unsigned short get_well_known_port() const;

    /// Parses and assigns an URI from the given string. Throws a
    /// SyntaxException if the uri is not valid.
    void parse(const std::string& uri);

    /// Parses and sets the user-info, host and port from the given data.
    void parse_authority(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Parses and sets the host and port from the given data.
    void parse_host_and_port(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Parses and sets the path from the given data.
    void parse_path(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Parses and sets the path, query and fragment from the given data.
    void parse_path_etc(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Parses and sets the query from the given data.
    void parse_query(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Parses and sets the fragment from the given data.
    void parse_fragment(std::string::const_iterator& it, const std::string::const_iterator& end);

    /// Appends a path to the URI's path.
    void merge_path(const std::string& path);

    /// Removes all dot segments from the path.
    void remove_dot_segments(bool removeLeading = true);

    /// Places the single path segments (delimited by slashes) into the
    /// given vector.
    static void get_path_segments(const std::string& path, std::vector<std::string>& segments);

    /// Builds the path from the given segments.
    void build_path(const std::vector<std::string>& segments, bool leadingSlash, bool trailingSlash);

private:
    std::string scheme_;
    std::string user_info_;
    std::string host_;
    unsigned short port_{0};
    std::string path_;
    std::string query_;
    std::string fragment_;
};

//
// inlines
//
inline const std::string& URI::get_scheme() const
{
    return scheme_;
}

inline const std::string& URI::get_user_info() const
{
    return user_info_;
}

inline const std::string& URI::get_host() const
{
    return host_;
}

inline const std::string& URI::get_path() const
{
    return path_;
}

inline const std::string& URI::get_raw_query() const
{
    return query_;
}

inline const std::string& URI::get_fragment() const
{
    return fragment_;
}

inline void swap(URI& u1, URI& u2)
{
    u1.swap(u2);
}

} // namespace

#endif // URI_H_INCLUDED
