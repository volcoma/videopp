#include <memory>

#ifndef HTMLPARSER_HPP_
#define HTMLPARSER_HPP_

#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

using std::enable_shared_from_this;
using std::shared_ptr;
using std::weak_ptr;

/**
 * class HtmlElement
 * HTML Element struct
 */
class html_element : public enable_shared_from_this<html_element>
{
public:
	friend class html_parser;
	friend class html_document;

public:


public:
	html_element() = default;

	html_element(const shared_ptr<html_element>& p)
		: parent_(p)
	{
	}

	std::string get_attribute(const std::string& k)
	{
		if(attributes_.find(k) != attributes_.end())
		{
			return attributes_[k];
		}

		return {};
	}

	shared_ptr<html_element> get_element_by_id(const std::string& id)
	{
		for(const auto& child : children_)
		{
			if(child->get_attribute("id") == id)
				return child;

			auto r = child->get_element_by_id(id);
			if(r)
				return r;
		}

		return {};
	}

	std::unordered_set<shared_ptr<html_element>> get_element_by_class_name(const std::string& name)
	{
		std::unordered_set<shared_ptr<html_element>> result;
		get_element_by_class_name(name, result);
		return result;
	}

	std::unordered_set<shared_ptr<html_element>> get_element_by_tag_name(const std::string& name)
	{
		std::unordered_set<shared_ptr<html_element>> result;
		get_element_by_tag_name(name, result);
		return result;
	}

	void select_element(const std::string& rule, std::unordered_set<shared_ptr<html_element>>& result)
	{
		if(rule.empty() || rule.at(0) != '/' || name_ == "plain")
			return;
		std::string::size_type pos = 0;
		if(rule.size() >= 2 && rule.at(1) == '/')
		{
			std::unordered_set<shared_ptr<html_element>> temp;
			get_all_elements(temp);
			pos = 1;
			std::string next = rule.substr(pos);
			if(next.empty())
			{
				for(const auto & i : temp)
				{
					result.insert(i);
				}
			}
			else
			{
				for(const auto & i : temp)
				{
					i->select_element(next, result);
				}
			}
		}
		else
		{
			std::string::size_type p = rule.find('/', 1);
			std::string line;
			if(p == std::string::npos)
			{
				line = rule;
				pos = rule.size();
			}
			else
			{
				line = rule.substr(0, p);
				pos = p;
			}

			enum
			{
				x_ele,
				x_wait_attr,
				x_attr,
				x_val
			};
			std::string ele, attr, oper, val, cond;
			int state = x_ele;
			for(p = 1; p < pos;)
			{
				char c = line.at(p++);
				switch(state)
				{
					case x_ele:
					{
						if(c == '@')
						{
							state = x_attr;
						}
						else if(c == '!')
						{
							state = x_wait_attr;
							cond.append(1, c);
						}
						else if(c == '[')
						{
							state = x_wait_attr;
						}
						else
						{
							ele.append(1, c);
						}
					}
					break;

					case x_wait_attr:
					{
						if(c == '@')
							state = x_attr;
						else if(c == '!')
						{
							cond.append(1, c);
						}
					}
					break;

					case x_attr:
					{
						if(c == '!')
						{
							oper.append(1, c);
						}
						else if(c == '=')
						{
							oper.append(1, c);
							state = x_val;
						}
						else if(c == ']')
						{
							state = x_ele;
						}
						else
						{
							attr.append(1, c);
						}
					}
					break;

					case x_val:
					{
						if(c == ']')
						{
							state = x_ele;
						}
						else
						{
							val.append(1, c);
						}
					}
					break;
				}
			}

			if(!val.empty() && val.at(0) == '\'')
			{
				val.erase(val.begin());
			}

			if(!val.empty() && val.at(val.size() - 1) == '\'')
			{
				val.pop_back();
			}

			bool matched = true;
			if(!ele.empty())
			{
				if(name_ != ele)
				{
					matched = false;
				}
			}

			if(cond == "!")
			{
				if(!attr.empty() && matched)
				{
					if(!oper.empty())
					{
						std::string v = attributes_[attr];
						if(oper == "=")
						{
							if(v == val)
								matched = false;
							if(attr == "class")
							{
								auto attr_class = split_class_name(get_attribute("class"));
								if(attr_class.find(val) != attr_class.end())
									matched = false;
							}
						}
						else if(oper == "!=")
						{
							if(v == val)
								matched = false;
							if(attr == "class")
							{
								auto attr_class = split_class_name(get_attribute("class"));
								if(attr_class.find(val) == attr_class.end())
									matched = false;
							}
						}
					}
					else
					{
						if(attributes_.find(attr) != attributes_.end())
							matched = false;
					}
				}
			}
			else
			{
				if(!attr.empty() && matched)
				{
					if(attributes_.find(attr) == attributes_.end())
					{
						matched = false;
					}
					else
					{
						std::string v = attributes_[attr];
						if(oper == "=")
						{
							if(v != val)
								matched = false;
							if(attr == "class")
							{
								auto attr_class = split_class_name(get_attribute("class"));
								if(attr_class.find(val) == attr_class.end())
									matched = false;
							}
						}
						else if(oper == "!=")
						{
							if(v == val)
								matched = false;
							if(attr == "class")
							{
								auto attr_class = split_class_name(get_attribute("class"));
								if(attr_class.find(val) != attr_class.end())
									matched = false;
							}
						}
					}
				}
			}

			std::string next = rule.substr(pos);
			if(matched)
			{
				if(next.empty())
				{
					result.insert(shared_from_this());
				}
				else
				{
					for(const auto& child : children_)
					{
						child->select_element(next, result);
					}
				}
			}
		};
	}

	shared_ptr<html_element> get_parent()
	{
		return parent_.lock();
	}

	const std::string& get_value()
	{
		if(value_.empty() && children_.size() == 1 && children_[0]->get_name() == "plain")
		{
			return children_[0]->get_value();
		}

		return value_;
	}

	const std::string& get_name()
	{
		return name_;
	}

	std::string text()
	{
		std::string str;
		plain_stylize(str);
		return str;
	}

	void plain_stylize(std::string& str)
	{
		if(name_ == "head" || name_ == "meta" || name_ == "style" || name_ == "script" || name_ == "link")
		{
			return;
		}

		if(name_ == "plain")
		{
			str.append(value_);
			return;
		}

		for(size_t i = 0; i < children_.size();)
		{
			children_[i]->plain_stylize(str);

			if(++i < children_.size())
			{
				std::string ele = children_[i]->get_name();
				if(ele == "td")
				{
					str.append("\t");
				}
				else if(ele == "tr" || ele == "br" || ele == "div" || ele == "p" || ele == "hr" ||
						ele == "area" || ele == "h1" || ele == "h2" || ele == "h3" || ele == "h4" ||
						ele == "h5" || ele == "h6" || ele == "h7")
				{
					str.append("\n");
				}
			}
		}
	}

	std::string html()
	{
		std::string str;
		html_stylize(str);
		return str;
	}

	void html_stylize(std::string& str)
	{
		if(name_.empty())
		{
			for(const auto& child : children_)
			{
				child->html_stylize(str);
			}

			return;
		}
		else if(name_ == "plain")
		{
			str.append(value_);
			return;
		}

		str.append("<" + name_);
		std::map<std::string, std::string>::const_iterator it = attributes_.begin();
		for(; it != attributes_.end(); it++)
		{
			str.append(" " + it->first + "=\"" + it->second + "\"");
		}
		str.append(">");

		if(children_.empty())
		{
			str.append(value_);
		}
		else
		{
            for(const auto& child : children_)
			{
				child->html_stylize(str);
			}
		}

		str.append("</" + name_ + ">");
	}

	const std::map<std::string, std::string>& get_attributes() const
	{
		return attributes_;
	}

	const std::vector<shared_ptr<html_element>> get_children() const
	{
		return children_;
	}

private:
	void get_element_by_class_name(const std::string& name, std::unordered_set<shared_ptr<html_element>>& result)
	{
		for(const auto& child : children_)
		{
			auto attr_class = split_class_name(child->get_attribute("class"));
			auto class_name = split_class_name(name);

			auto iter = class_name.begin();
			for(; iter != class_name.end(); ++iter)
			{
				if(attr_class.find(*iter) == attr_class.end())
				{
					break;
				}
			}

			if(iter == class_name.end())
			{
				result.insert(child);
			}

			child->get_element_by_class_name(name, result);
		}
	}

	void get_element_by_tag_name(const std::string& name, std::unordered_set<shared_ptr<html_element>>& result)
	{
		for(const auto& child : children_)
		{
			if(child->name_ == name)
				result.insert(child);

			child->get_element_by_tag_name(name, result);
		}
	}

	void get_all_elements(std::unordered_set<shared_ptr<html_element>>& result)
	{
		for(auto& child : children_)
		{
			child->get_all_elements(result);
			result.insert(child);
		}
	}

	void parse(const std::string& attr)
	{
		size_t index = 0;
		std::string k;
		std::string v;
		char split = ' ';
		bool quota = false;

		enum parse_attr_state
		{
			PARSE_ATTR_KEY,
			PARSE_ATTR_VALUE_BEGIN,
			PARSE_ATTR_VALUE_END,
		};

		parse_attr_state state = PARSE_ATTR_KEY;

		while(attr.size() > index)
		{
			char input = attr.at(index);
			switch(state)
			{
				case PARSE_ATTR_KEY:
				{
					if(input == '\t' || input == '\r' || input == '\n')
					{
					}
					else if(input == '\'' || input == '"')
					{
						std::cerr << "WARN : attribute unexpected " << input << std::endl;
					}
					else if(input == ' ')
					{
						if(!k.empty())
						{
							attributes_[k] = v;
							k.clear();
						}
					}
					else if(input == '=')
					{
						state = PARSE_ATTR_VALUE_BEGIN;
					}
					else
					{
						k.append(attr.c_str() + index, 1);
					}
				}
				break;

				case PARSE_ATTR_VALUE_BEGIN:
				{
					if(input == '\t' || input == '\r' || input == '\n' || input == ' ')
					{
						if(!k.empty())
						{
							attributes_[k] = v;
							k.clear();
						}
						state = PARSE_ATTR_KEY;
					}
					else if(input == '\'' || input == '"')
					{
						split = input;
						quota = true;
						state = PARSE_ATTR_VALUE_END;
					}
					else
					{
						v.append(attr.c_str() + index, 1);
						quota = false;
						state = PARSE_ATTR_VALUE_END;
					}
				}
				break;

				case PARSE_ATTR_VALUE_END:
				{
					if((quota && input == split) ||
					   (!quota && (input == '\t' || input == '\r' || input == '\n' || input == ' ')))
					{
						attributes_[k] = v;
						k.clear();
						v.clear();
						state = PARSE_ATTR_KEY;
					}
					else
					{
						v.append(attr.c_str() + index, 1);
					}
				}
				break;
			}

			index++;
		}

		if(!k.empty())
		{
			attributes_[k] = v;
		}

		// trim
		if(!value_.empty())
		{
			value_.erase(0, value_.find_first_not_of(' '));
			value_.erase(value_.find_last_not_of(' ') + 1);
		}
	}

	static std::set<std::string> split_class_name(const std::string& name)
	{
#if defined(WIN32)
#define strtok_ strtok_s
#else
#define strtok_ strtok_r
#endif
		std::set<std::string> class_names;
		char* temp = nullptr;
		char* p = strtok_((char*)name.c_str(), " ", &temp);
		while(p)
		{
			class_names.insert(p);
			p = strtok_(nullptr, " ", &temp);
		}

		return class_names;
	}

private:
    std::string name_{};
    std::string value_{};
    std::map<std::string, std::string> attributes_{};
    weak_ptr<html_element> parent_{};
    std::vector<shared_ptr<html_element>> children_{};
};

/**
 * class HtmlDocument
 * Html Doc struct
 */
class html_document
{
public:
	html_document(shared_ptr<html_element>& root)
		: root_(root)
	{
	}

	shared_ptr<html_element> get_element_by_id(const std::string& id)
	{
		return root_->get_element_by_id(id);
	}

	std::unordered_set<shared_ptr<html_element>> get_element_by_class_name(const std::string& name)
	{
		return root_->get_element_by_class_name(name);
	}

	std::unordered_set<shared_ptr<html_element>> get_element_by_tag_name(const std::string& name)
	{
		return root_->get_element_by_tag_name(name);
	}

	std::unordered_set<shared_ptr<html_element>> select_element(const std::string& rule)
	{
		std::unordered_set<shared_ptr<html_element>> result;
		for(const auto& child : root_->get_children())
		{
			child->select_element(rule, result);
		}

		return result;
	}

	std::string html()
	{
		return root_->html();
	}

	std::string text()
	{
		return root_->text();
	}

	shared_ptr<html_element> root()
	{
		return root_;
	}

private:
    shared_ptr<html_element> root_{};
};

/**
 * class HtmlParser
 * html parser and only one interface
 */
class html_parser
{
public:
	html_parser()
	{
		static const std::string token[] = {"br",	"hr",	 "img",   "input",   "link",  "meta",
											"area",  "base",   "col",   "command", "embed", "keygen",
											"param", "source", "track", "wbr"};
		self_closing_tags_.insert(token, token + sizeof(token) / sizeof(token[0]));
	}

	/**
	 * parse html by C-Style data
	 * @param data
	 * @param len
	 * @return html document object
	 */
	shared_ptr<html_document> parse(const char* data, size_t len)
	{
		stream_ = data;
		length_ = len;
		size_t index = 0;
		root_.reset(new html_element());
		while(length_ > index)
		{
			char input = stream_[index];
			if(input == '\r' || input == '\n' || input == '\t' || input == ' ')
			{
				index++;
			}
			else if(input == '<')
			{
				index = parse_element(index, root_);
			}
			else
			{
				break;
			}
		}

		return std::make_shared<html_document>(root_);
	}

	/**
	 * parse html by string data
	 * @param data
	 * @return html document object
	 */
	shared_ptr<html_document> parse(const std::string& data)
	{
		return parse(data.data(), data.size());
	}

private:
	size_t parse_element(size_t index, shared_ptr<html_element>& element)
	{
		while(length_ > index)
		{
			char input = stream_[index + 1];
			if(input == '!')
			{
				if(strncmp(stream_ + index, "<!--", 4) == 0)
				{
					return skip_until(index + 2, "-->");
				}
				else
				{
					return skip_until(index + 2, '>');
				}
			}
			else if(input == '/')
			{
				return skip_until(index, '>');
			}
			else if(input == '?')
			{
				return skip_until(index, "?>");
			}

			shared_ptr<html_element> self(new html_element(element));

			enum parse_element_state
			{
				PARSE_ELEMENT_TAG,
				PARSE_ELEMENT_ATTR,
				PARSE_ELEMENT_VALUE,
				PARSE_ELEMENT_TAG_END
			};

			parse_element_state state = PARSE_ELEMENT_TAG;
			index++;
			std::string attr;

			while(length_ > index)
			{
				switch(state)
				{
					case PARSE_ELEMENT_TAG:
					{
						char input = stream_[index];
						if(input == ' ' || input == '\r' || input == '\n' || input == '\t')
						{
							if(!self->name_.empty())
							{
								state = PARSE_ELEMENT_ATTR;
							}
							index++;
						}
						else if(input == '/')
						{
							self->parse(attr);
							element->children_.push_back(self);
							return skip_until(index, '>');
						}
						else if(input == '>')
						{
							if(self_closing_tags_.find(self->name_) != self_closing_tags_.end())
							{
								element->children_.push_back(self);
								return ++index;
							}
							state = PARSE_ELEMENT_VALUE;
							index++;
						}
						else
						{
							self->name_.append(stream_ + index, 1);
							index++;
						}
					}
					break;

					case PARSE_ELEMENT_ATTR:
					{
						char input = stream_[index];
						if(input == '>')
						{
							if(stream_[index - 1] == '/')
							{
								attr.erase(attr.size() - 1);
								self->parse(attr);
								element->children_.push_back(self);
								return ++index;
							}
							else if(self_closing_tags_.find(self->name_) != self_closing_tags_.end())
							{
								self->parse(attr);
								element->children_.push_back(self);
								return ++index;
							}
							state = PARSE_ELEMENT_VALUE;
							index++;
						}
						else
						{
							attr.append(stream_ + index, 1);
							index++;
						}
					}
					break;

					case PARSE_ELEMENT_VALUE:
					{
						if(self->name_ == "script" || self->name_ == "noscript" || self->name_ == "style")
						{
							std::string close = "</" + self->name_ + ">";

							size_t pre = index;
							index = skip_until(index, close.c_str());
							if(index > (pre + close.size()))
								self->value_.append(stream_ + pre, index - pre - close.size());

							self->parse(attr);
							element->children_.push_back(self);
							return index;
						}

						char input = stream_[index];
						if(input == '<')
						{
							if(!self->value_.empty())
							{
								shared_ptr<html_element> child(new html_element(self));
								child->name_ = "plain";
								child->value_.swap(self->value_);
								self->children_.push_back(child);
							}

							if(stream_[index + 1] == '/')
							{
								state = PARSE_ELEMENT_TAG_END;
							}
							else
							{
								index = parse_element(index, self);
							}
						}
						else if(input != '\r' && input != '\n' && input != '\t')
						{
							self->value_.append(stream_ + index, 1);
							index++;
						}
						else
						{
							index++;
						}
					}
					break;

					case PARSE_ELEMENT_TAG_END:
					{
						index += 2;
						std::string selfname = self->name_ + ">";
						if(strncmp(stream_ + index, selfname.c_str(), selfname.size()))
						{
							size_t pre = index;
							index = skip_until(index, ">");
							std::string value;
							if(index > (pre + 1))
								value.append(stream_ + pre, index - pre - 1);
							else
								value.append(stream_ + pre, index - pre);

							shared_ptr<html_element> parent = self->get_parent();
							while(parent)
							{
								if(parent->name_ == value)
								{
									std::cerr << "WARN : element not closed <" << self->name_ << "> "
											  << std::endl;
									self->parse(attr);
									element->children_.push_back(self);
									return pre - 2;
								}

								parent = parent->get_parent();
							}

							std::cerr << "WARN : unexpected closed element </" << value << "> for <"
									  << self->name_ << ">" << std::endl;
							state = PARSE_ELEMENT_VALUE;
						}
						else
						{
							self->parse(attr);
							element->children_.push_back(self);
							return skip_until(index, '>');
						}
					}
					break;
				}
			}
		}

		return index;
	}

	size_t skip_until(size_t index, const char* data)
	{
		while(length_ > index)
		{
			if(strncmp(stream_ + index, data, strlen(data)) == 0)
			{
				return index + strlen(data);
			}
			else
			{
				index++;
			}
		}

		return index;
	}

	size_t skip_until(size_t index, const char data)
	{
		while(length_ > index)
		{
			if(stream_[index] == data)
			{
				return ++index;
			}
			else
			{
				index++;
			}
		}

		return index;
	}

private:
    const char* stream_{};
    size_t length_{};
    std::set<std::string> self_closing_tags_{};
    shared_ptr<html_element> root_{};
};

#endif
