#include "Client.hpp"

Client::Client(int fd, fd_set *rset, fd_set *wset, std::string ip) : _req(ip)
{
    _fd = fd;
    write_fd = -1;
    read_fd = -1;
    cgi_pid = -1;
	autoidx_flag = -1;
	tmp_fd = -1;
    _rset = rset;
    _wset = wset;

    fcntl(fd, F_SETFL, O_NONBLOCK);
    FT_FD_SET(fd, _rset);

    _rbuf = "";
    _time = get_time();
    _status = 0;
}

Client::~Client()
{
    if (_fd != -1)
    {
        close(_fd);
        FT_FD_CLR(_fd, _rset);
        FT_FD_CLR(_fd, _wset);
    }
	if (read_fd != -1)
	{
		close(read_fd);
		FT_FD_CLR(read_fd, _rset);
	}
	if (write_fd != -1)
	{
		close(write_fd);
		FT_FD_CLR(write_fd, _wset);	
	}
	if (tmp_fd != -1)
		close(tmp_fd);

	std::cout << "Bye bye Client~" << std::endl;
}

Client::Client(const Client &copy) : _req("")
{
    *this = copy;
}

Client& Client::operator=(const Client &other)
{
    if (this == &other)
        return (*this);

    _req = other._req;
    _fd = other._fd;
    _rset = other._rset;
    _wset = other._wset;
    _rbuf = other._rbuf;
    _res_msg = other._res_msg;
    _time = other._time;

    return (*this);
}

int Client::get_fd(void)
{
    return (_fd);
}

void	Client::set_write_file(bool flag)
{
	if (write_fd != -1)
	{
		if (flag)
			FT_FD_SET(write_fd, _wset);
		else
			FT_FD_CLR(write_fd, _wset);
	}
}

void	Client::set_read_file(bool flag)
{
	if (read_fd != -1)
	{
		if (flag)
			FT_FD_SET(read_fd, _rset);
		else
			FT_FD_CLR(read_fd, _rset);
	}
}

void Client::write_file(void)
{
    int ret = 0;

	ret = write(write_fd, _req.get_body().c_str(), _req.get_body().size());
	if (ret == -1)
	{
		std::cerr << "WRITE ERROR" << std::endl;
		_req.get_body().clear();
		close(write_fd);
		FT_FD_CLR(write_fd, _wset);
		write_fd = -1;
	}

	if ((unsigned long)ret < _req.get_body().size())
		_req.get_body() = _req.get_body().substr(ret);
	else
	{
		_req.get_body().clear();
		close(write_fd);
		FT_FD_CLR(write_fd, _wset);
		write_fd = -1;
	}
}

void Client::read_file(void)
{
    char			buffer[BUFFER_SIZE + 1];
	int				status = 0;

	if (cgi_pid != -1)
	{
		if (waitpid(cgi_pid, &status, 0) == 0)
			return ;
		else
		{
			if ((status >> 8) == 1)
			{
				close(tmp_fd);
				tmp_fd = -1;
				cgi_pid = -1;
				unlink("cgi.txt");
				close(read_fd);
				FT_FD_CLR(read_fd, _rset);
				read_fd = -1;
				_res._body = "CGI_ERROR\n";
				return ;
			}
		}
	}

    int ret = read(read_fd, buffer, BUFFER_SIZE);
	
	buffer[ret] = '\0';
	std::string	tmp(buffer, ret);
	_res._body += tmp;
	
	if (ret == 0)
	{
		close(read_fd);
		unlink("cgi.txt");
		FT_FD_CLR(read_fd, _rset);
		read_fd = -1;
	}
	else if (ret == -1)
	{
		close(read_fd);
		unlink("cgi.txt");
		FT_FD_CLR(read_fd, _rset);
		read_fd = -1;
		std::cerr << "READ ERROR!" << std::endl;
		_res._body = "READ_ERROR\n";
	}
}
