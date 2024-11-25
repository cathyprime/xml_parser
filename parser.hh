#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

class XMLNode;

class XMLNodelist {
    size_t capacity;
    XMLNode **data;
    size_t len;
    void resize();

  public:
    XMLNode *operator[](size_t idx);
    XMLNodelist();
    size_t size() const;
    void print(int x);
    void append(XMLNode *node);
    ~XMLNodelist();
};

struct XMLArg {
    char *key;
    char *value;

    XMLArg() :
      key(nullptr),
      value(nullptr)
    { }

    XMLArg(char *m_key, char *m_value) :
      key(m_key),
      value(m_value)
    { }

    XMLArg(XMLArg &&other) :
      key(other.key),
      value(other.value)
    {
        other.key = nullptr;
        other.value = nullptr;
    }

    void operator=(XMLArg &&rhs)
    {
        if (key) free(key);
        if (value) free(value);
        key = rhs.key;
        value = rhs.value;
        rhs.key = nullptr;
        rhs.value = nullptr;
    }

    ~XMLArg()
    {
        if (key) free(key);
        if (value) free(value);
    }
};

struct XMLArglist {
    size_t capacity;
    size_t len;
    XMLArg *data;

    void resize()
    {
        capacity *= 2;
        XMLArg *new_data = new XMLArg[capacity];
        memcpy(new_data, data, capacity * sizeof(XMLNode*));

        delete[] data;
        data = new_data;
    }

  public:
    XMLArg *operator[](size_t idx)
    {
        return &data[idx];
    }

    XMLArglist() :
      capacity(1),
      len(0)
    {
        data = new XMLArg[capacity];
    }

    size_t size() const
    {
        return len;
    }

    void print(int padding)
    {
        for (size_t i = 0; i < len; ++i) {
            printf("%*sarg: { key = %s, value = %s }\n", padding, "", data[i].key, data[i].value);
        }
    }

    void append(XMLArg *arg)
    {
        if (len >= capacity) resize();
        data[len++] = static_cast<XMLArg&&>(*arg);
    }

    ~XMLArglist()
    {
        if (data) delete[] data;
    }
};

struct XMLNode {
    char *tag;
    char *inner_text;
    XMLNode *parent;
    XMLNodelist children;
    XMLArglist arguments;

    XMLNode(XMLNode *parent) :
      tag(nullptr),
      inner_text(nullptr),
      parent(parent),
      children(XMLNodelist{})
    { }

    ~XMLNode()
    {
        if (tag) free(tag);
        if (inner_text) free(inner_text);
    }

    XMLNode *spawn_child()
    {
        auto new_child = new XMLNode{this};
        children.append(new_child);
        return new_child;
    }

    friend class XMLNodelist;
    void print(int padding = 0)
    {
        printf("%*s%s: %s\n", padding, "", tag, inner_text);
        arguments.print(padding + 2);
        children.print(padding == 0 ? 2 : padding + 2);
    }
};

inline void XMLNodelist::print(int padding)
{
    for (size_t i = 0; i < len; ++i)
        data[i]->print(padding);
}

inline void XMLNodelist::resize()
{
    auto new_data = new XMLNode*[capacity*2];
    memcpy(new_data, data, capacity * sizeof(XMLNode*));
    delete[] data;
    data = new_data;
    capacity *= 2;
}

inline XMLNodelist::XMLNodelist():
  capacity(1),
  data(new XMLNode *[capacity]),
  len(0)
{ }

inline void XMLNodelist::append(XMLNode *node)
{
    if (capacity - len <= 0) resize();
    data[len] = node;
    len++;
}

inline XMLNodelist::~XMLNodelist()
{
    for (size_t i = 0; i < len; ++i)
        if (data[i]) delete data[i];
    if (data) delete[] data;
}

inline XMLNode *XMLNodelist::operator[](size_t idx)
{
    return data[idx];
}

inline size_t XMLNodelist::size() const
{
    return len;
}

struct XMLDocument {
    XMLNode *root;

    ~XMLDocument()
    {
        if (root) delete root;
    }
};

namespace {

    char *trim_whitespace(char *str)
    {
        while (isspace(*str)) str++;

        size_t len = strlen(str);
        for (size_t i = len; i > 0; --i) {
            if (str[i] != '\0' && !isspace(str[i])) {
                if (i != len)
                    str[i+1] = '\0';
                break;
            }
        }

        return str;
    }

    char *dupstr(const char *str)
    {
        if (strlen(str) == 0) return nullptr;
        return strdup(str);
    }

    enum class State {
        Normal,
        Tag,
        EndTag,
    };

    class XMLParser {
      public:
        char *buf;
      private:
        size_t len;
        XMLDocument *doc;
        char lex[256];
        size_t lexi;
        size_t i;
        XMLNode *current_node;
        State state = State::Normal;

        bool parse_args(uint8_t spaces)
        {
            if (lexi >= 256) return false;

            size_t start = lexi;
            size_t eq_pos;

            while (spaces > 0) {
                size_t end = start;

                while (!isspace(lex[start])) {
                    if (lex[start] == '=') eq_pos = start;
                    start--;
                }
                lex[start] = '\0';
                lex[eq_pos] = '\0';
                lex[end + 1] = '\0';
                start++;
                XMLArg arg = {
                    strdup(trim_whitespace(&lex[start])),
                    strdup(trim_whitespace(&lex[eq_pos + 1]))
                };
                current_node->arguments.append(&arg);
                lexi = start - 1;
                spaces--;
            }

            return true;
        }

        bool parse_tag()
        {
            if (lex[0] != '\0' && lexi != 0) {
                if (!current_node) {
                    fprintf(stderr, "Freestanding text is not allowed!");
                    return false;
                }

                lex[lexi] = '\0';
                char *text = trim_whitespace(lex);
                if (strlen(text) > 1) {
                    if (!current_node->inner_text) {
                        current_node->inner_text = dupstr(text);
                    } else {
                        size_t in_len = strlen(current_node->inner_text);
                        size_t t_len = strlen(text);
                        current_node->inner_text = (char*)realloc(current_node->inner_text, in_len + t_len + 2);
                        current_node->inner_text[in_len] = '\n';
                        current_node->inner_text[in_len + 1] = '\0';
                        strcat(current_node->inner_text, text);
                    }
                }
                lexi = 0;
            }

            uint8_t spaces = 0;
            while (buf[i] != '>') {
                if (isspace(buf[i])) spaces++;
                lex[lexi++] = buf[i++];
            }

            lex[lexi] = '\0';

            if (state == State::Tag) {
                if (!current_node)
                    current_node = doc->root;
                else
                    current_node = current_node->spawn_child();

                if (spaces != 0) parse_args(spaces);

                if (!current_node->tag)
                    current_node->tag = dupstr(lex);
                lexi = 0;
                return true;
            }

            if (state == State::EndTag) {
                if (spaces != 0) {
                    fprintf(stderr, "closing tags shouldn't have arguments\n");
                    return false;
                }

                if (!current_node) {
                    fprintf(stderr, "shouldn't start with closing tag\n");
                    return false;
                }

                if (!current_node->tag) {
                    fprintf(stderr, "empty tag?\n");
                    state = State::Normal;
                    return false;
                }

                if (strcmp(current_node->tag, lex) != 0) {
                    fprintf(stderr, "tags don't match! (%s != %s)\n", current_node->tag, lex);
                    return false;
                }
                current_node = current_node->parent;
                state = State::Normal;
                return true;
            }

            return false;
        }

        void parse_normal()
        {
            if (buf[i] == '<' || buf[i] == '>') {
                i++;
                return;
            }
            lex[lexi++] = buf[i++];
        }

      public:
        XMLParser(XMLDocument *doc, size_t sz) :
          buf(new char[sz]), len(sz),
          doc(doc), lex(),
          lexi(0), i(0), current_node(nullptr)
        {
            doc->root = new XMLNode{nullptr};
        }

        bool parse()
        {
            while (i < len && buf[i] != '\0') {
                switch (buf[i]) {
                    case '<': {
                        if (buf[i+1] == '/') {
                            state = State::EndTag;
                            i++;
                        } else {
                            state = State::Tag;
                        }
                        i++;
                        if (!(parse_tag()))
                            return false;
                    } break;
                    case '>':
                        state = State::Normal;
                        lexi = 0;
                    default:
                        parse_normal();
                        break;
                }
            }
            return true;
        }

        ~XMLParser()
        {
            delete[] buf;
        }
    };

}

inline bool load_file(XMLDocument *doc, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    XMLParser parser = {doc, size};
    fread(parser.buf, 1, size, f);
    fclose(f);


    return parser.parse();
}
