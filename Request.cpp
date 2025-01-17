#include "Request.hpp"

Request::Request()
{}

Request::~Request()
{}

void	Request::clear()
{
    _error_code = -1;
    _method = -1;
    _uri.clear();
    _headers.clear();
    _body.clear();
    _method_str.clear();
    _conf.clear();
    _client_ip.clear();
    _limit_body_size = -1;
    _chunk_len = -1;
}

Request::Request(std::string client_ip)
{
    _client_ip = client_ip;
	_error_code = -1;
	_chunk_len = -1;
	_body = "";
	_limit_body_size = -1;
}

Request::Request (const Request& copy)
{
    *this = copy;
};

Request& Request::operator=(const Request& other)
{
    if (this == &other)
        return (*this);

    _method = other._method;
    _method_str = other._method_str;
    _uri = other._uri;

    _headers = other._headers;
    _body = other._body;

    _error_code = other._error_code;
    _client_ip = other._client_ip;

    _conf = other._conf;
    _chunk_len = other._chunk_len;
	_limit_body_size = other._limit_body_size;

    return (*this);
}


void	Request::feed_conf(std::vector<conf> &conf_input)
{
    std::map<std::string, std::string> elem;
    std::string		tmp;
    struct stat		info;
    conf			to_parse;

    std::vector<conf>::iterator it = conf_input.begin();
    for(; it != conf_input.end(); ++it)
    {
        if (_headers["Host"] == (*it)["server|"]["server_name"])
        {
            to_parse = *it;
            break ;
        }
    }
    if (it == conf_input.end())
        to_parse = conf_input[0];
 
    if (_uri[0] != '/')
        _uri = "/" + _uri;

    tmp = _uri.substr(0, _uri.find('?'));
    do
    {
        if (to_parse.find("server|location " + tmp + "|") != to_parse.end())
        {
            elem = to_parse["server|location " + tmp + "|"];
            break ;
        }
        tmp = tmp.substr(0, tmp.find_last_of('/'));

    } while (tmp != "");

    if (elem.size() == 0)
    {
        if (to_parse.find("server|location /|") != to_parse.end())
            elem = to_parse["server|location /|"];
    }
	_conf = elem;

	_conf["path"] = _uri.substr(0, _uri.find("?"));
	if (elem.find("root") != elem.end())
		_conf["path"].replace(0, tmp.size(), elem["root"]);

	std::vector<std::string> tokens;
	tokens = split(_conf["method_allowed"], ' ');
    int num = tokens.size();
    for (int i = 0; i < num; ++i)
    {
        if (tokens[i] == _method_str)
            break ;
        else if (i == num - 1)
        {
            _error_code = 405;
            break ;
        }
    }

	for (std::map<std::string, std::string>::iterator it(to_parse["server|"].begin()); it != to_parse["server|"].end(); ++it)
    {
        if (_conf.find(it->first) == _conf.end())
            _conf[it->first] = it->second;
    }
    lstat(_conf["path"].c_str(), &info);
    if (FT_S_ISDIR(info.st_mode)) //directory면 default index page open
    {
        if (_conf["index"][0] && _conf["autoindex"] != "on")
            _conf["path"] += "/" + elem["index"];
    }
	if (_conf["path"] == "//")
		_conf["path"] = _conf["index"];
	if (_conf.find("limit_body_size") != _conf.end())
        _limit_body_size = std::stoi(_conf["limit_body_size"]);
    
    // std::cout << "============"<< std::endl;
    // for(std::map<std::string, std::string>::iterator it = _conf.begin(); it != _conf.end(); ++it)
    // 	std::cout << it->first << " " << it->second << std::endl;
    // std::cout << "============"<< std::endl;

    if (_headers.find("Content-Length") != _headers.end() && _conf.find("limit_body_size") != _conf.end())
    {
        if (std::stoi(_headers["Content-Length"]) > std::stoi(_conf["limit_body_size"]))
            _error_code = 413;
    }

    if (stat(_conf["path"].c_str(), &info) == -1 && _method != PUT && _method != POST)
        _error_code = 404;

    if (_conf.find("auth") != _conf.end())
    {
        _error_code = 401;
        if (_headers.find("Authorization") != _headers.end())
        {
            std::string credentials = decode_base_64(_headers["Authorization"].c_str());
            if (ft_strcmp(credentials.c_str(), _conf["auth"].c_str()) == 0)
                _error_code = 200;
        }
    }
}

void	Request::parse_header(std::string &req)
{
    std::string line;
    std::size_t pos;
    std::string key;
    std::string value;

    while (!req.empty())
    {
        ft_getline(req, line);
        pos = line.find(":");
        if (pos != std::string::npos)
        {
            key = trim(line.substr(0, pos));
            value = trim(line.substr(pos + 1));

            if (key.empty() || value.empty())
                break ;
            
            _headers[key] = value;
        }
        else
            break ;
    }
}

int		Request::parse_request(std::string &req, std::vector<conf> &conf)
{
    std::string line;
	size_t pos;

	if (_error_code == -1)
	{
        
		if ((pos = req.find("\r\n\r\n")) == std::string::npos)
			return (0);
		line = req.substr(0, pos);
		_error_code = 200;
		if (req[0] == '\r')
			req.erase(req.begin());
		if (req[0] == '\n')
			req.erase(req.begin());

		ft_getline(req, line);
		parse_first_line(line); // feed method and uri. version is fixed as HTTP /1.1
		parse_header(req);
		if (_uri != "*" || _uri != "OPTIONS")
			feed_conf(conf);
	}
	return (parse_body(req));
}

int		Request::parse_body(std::string &req)
{
    if (_headers.find("Transfer-Encoding") != _headers.end() && ft_strncmp(_headers["Transfer-Encoding"].c_str(), "chunked", 7) == 0)
        return parse_chunk(req);
    else if (_headers.find("Content-Length") != _headers.end())
    {
        //if (req.find("\r\n\r\n") != std::string::npos)
        {
            unsigned long len = std::stoi(_headers["Content-Length"]);
            _body += req.substr(0, len);
            return (1);
        }

        return (0);
    }
    else
    {
        if (_method == POST || _method == PUT)
            _error_code = 411;
        return (1);
    }
}

int		Request::parse_chunk(std::string &req)
{
	std::string line;

	while (req.find("\r\n") != std::string::npos)
	{
		if (_chunk_len == -1)
		{
			ft_getline(req, line);//숫자 읽기
			_chunk_len = std::stoi(line, 0, 16);
			if (_chunk_len == 0)
				return (1);
		}
		else
		{
			ft_getline(req,line);
			_body += line.substr(0, _chunk_len);
			_chunk_len = -1;
		}
	}
	return (0);
}

void	Request::parse_first_line(std::string &line)
{
    std::vector<std::string> tokens = split(line, ' ');

    _method_str = tokens[0];
    if (tokens.size() != 3)
        _error_code = 400;
    else
    {
        if (!ft_strncmp("GET", tokens[0].c_str(), 3))
            _method = GET;
        else if (!ft_strncmp("POST", tokens[0].c_str(), 4))
            _method = POST;
        else if (!ft_strncmp("HEAD", tokens[0].c_str(), 4))
            _method = HEAD;
        else if (!ft_strncmp("PUT", tokens[0].c_str(), 3))
            _method = PUT;
        else if (!ft_strncmp("DELETE", tokens[0].c_str(), 6))
            _method = DELETE;
        else if (!ft_strncmp("CONNECT", tokens[0].c_str(), 7))
            _method = CONNECT;
        else if (!ft_strncmp("OPTIONS", tokens[0].c_str(), 7))
            _method = OPTIONS;
        else if (!ft_strncmp("TRACE", tokens[0].c_str(), 7))
            _method = TRACE;
        else
            _error_code = 400;

        _uri = tokens[1];

        if (strncmp("HTTP/1.1", tokens[2].c_str(), 8))
            _error_code = 505;

    }
}

static const int B64index[256] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,
0,  0,  0, 63,  0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };

std::string Request::decode_base_64(const char *data)
{
    while (*data != ' ')
		data++;
	data++;
	unsigned int len = ft_strlen(data);
	unsigned char* p = (unsigned char*)data;
    int pad = len > 0 && (len % 4 || p[len - 1] == '=');
    const size_t L = ((len + 3) / 4 - pad) * 4;
    std::string str(L / 4 * 3 + pad, '\0');

    for (size_t i = 0, j = 0; i < L; i += 4)
    {
        int n = B64index[p[i]] << 18 | B64index[p[i + 1]] << 12 | B64index[p[i + 2]] << 6 | B64index[p[i + 3]];
        str[j++] = n >> 16;
        str[j++] = n >> 8 & 0xFF;
        str[j++] = n & 0xFF;
    }
    if (pad)
    {
        int n = B64index[p[L]] << 18 | B64index[p[L + 1]] << 12;
        str[str.size() - 1] = n >> 16;

        if (len > L + 2 && p[L + 2] != '=')
        {
            n |= B64index[p[L + 2]] << 6;
            str.push_back(n >> 8 & 0xFF);
        }
    }
    if (str.back() == 0)
    	str.pop_back();

    return (str);
}

int Request::get_method(){return (_method);}
std::string Request::get_method_str(){return (_method_str);}
int	Request::get_error_code(){return (_error_code);}
std::string	Request::get_uri(){return (_uri);}
std::string	Request::get_body(){return (_body);}
std::string	Request::get_client_ip(){return (_client_ip);}
std::map<std::string, std::string>	Request::get_conf(){return (_conf);}
std::map<std::string, std::string>  Request::get_headers(){return (_headers);}
int	Request::get_limit(){return (_limit_body_size);}

void	Request::set_error_code(int n){_error_code = n;}
