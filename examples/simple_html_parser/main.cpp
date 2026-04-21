#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cctype>

// Forward declarations
class Node;
class ElementNode;
class TextNode;
class CommentNode;

// Type alias for node pointers
using NodePtr = std::shared_ptr<Node>;

// Base Node class
class Node {
public:
    virtual ~Node() = default;
    
    virtual std::string getType() const { return "Node"; }
    
    virtual void print(int indent = 0) const {
        std::string spaces(indent * 2, ' ');
        std::cout << spaces << getType() << std::endl;
    }
    
    virtual std::string toString() const { return ""; }
};

// TextNode class for text content
class TextNode : public Node {
private:
    std::string text;
    
public:
    explicit TextNode(const std::string& text) : text(text) {}
    
    std::string getType() const override { return "TextNode"; }
    
    void print(int indent = 0) const override {
        std::string spaces(indent * 2, ' ');
        std::cout << spaces << "TextNode: \"" << text << "\"" << std::endl;
    }
    
    std::string getText() const { return text; }
    
    std::string toString() const override { return text; }
};

// CommentNode class for HTML comments
class CommentNode : public Node {
private:
    std::string comment;
    
public:
    explicit CommentNode(const std::string& comment) : comment(comment) {}
    
    std::string getType() const override { return "CommentNode"; }
    
    void print(int indent = 0) const override {
        std::string spaces(indent * 2, ' ');
        std::cout << spaces << "CommentNode: \"" << comment << "\"" << std::endl;
    }
    
    std::string getComment() const { return comment; }
    
    std::string toString() const override { return "<!--" + comment + "-->"; }
};

// ElementNode class for HTML elements
class ElementNode : public Node {
private:
    std::string tagName;
    std::map<std::string, std::string> attributes;
    std::vector<NodePtr> children;
    
public:
    explicit ElementNode(const std::string& tagName) : tagName(tagName) {}
    
    std::string getType() const override { return "ElementNode"; }
    
    void print(int indent = 0) const override {
        std::string spaces(indent * 2, ' ');
        std::cout << spaces << "ElementNode: <" << tagName << ">";
        
        if (!attributes.empty()) {
            std::cout << " attributes=[";
            bool first = true;
            for (const auto& attr : attributes) {
                if (!first) std::cout << ", ";
                std::cout << attr.first << "=\"" << attr.second << "\"";
                first = false;
            }
            std::cout << "]";
        }
        std::cout << std::endl;
        
        for (const auto& child : children) {
            child->print(indent + 1);
        }
    }
    
    void addChild(NodePtr child) {
        children.push_back(child);
    }
    
    void setAttribute(const std::string& name, const std::string& value) {
        attributes[name] = value;
    }
    
    std::string getAttribute(const std::string& name) const {
        auto it = attributes.find(name);
        return (it != attributes.end()) ? it->second : "";
    }
    
    std::string getTagName() const { return tagName; }
    
    const std::vector<NodePtr>& getChildren() const { return children; }
    
    std::string toString() const override {
        std::string result = "<" + tagName;
        
        for (const auto& attr : attributes) {
            result += " " + attr.first + "=\"" + attr.second + "\"";
        }
        
        result += ">";
        
        for (const auto& child : children) {
            result += child->toString();
        }
        
        result += "</" + tagName + ">";
        
        return result;
    }
};

// Self-closing elements (void elements)
const std::vector<std::string> SELF_CLOSING_ELEMENTS = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"
};

// Common elements we'll focus on
const std::vector<std::string> COMMON_ELEMENTS = {
    "html", "head", "body", "title", "meta", "link", "style",
    "div", "span", "p", "a", "img", "br", "hr",
    "h1", "h2", "h3", "h4", "h5", "h6",
    "ul", "ol", "li",
    "table", "thead", "tbody", "tr", "td", "th",
    "form", "input", "button", "select", "option", "textarea",
    "label", "script", "header", "footer", "nav", "section", "article"
};

// HTML Parser class
class HTMLParser {
private:
    std::string htmlContent;
    size_t pos;
    std::vector<std::string> parseErrors;
    
    // Helper to trim whitespace
    std::string trim(const std::string& str) const {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
            start++;
        }
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
            end--;
        }
        return str.substr(start, end - start);
    }
    
    // Helper to convert string to lowercase
    std::string toLower(const std::string& str) const {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }
    
    // Check if we're at end of content
    bool atEnd() const {
        return pos >= htmlContent.size();
    }
    
    // Get current character
    char current() const {
        return atEnd() ? '\0' : htmlContent[pos];
    }
    
    // Advance position
    void advance() {
        pos++;
    }
    
    // Skip whitespace
    void skipWhitespace() {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(current()))) {
            advance();
        }
    }
    
    // Skip DOCTYPE declaration
    void skipDoctype() {
        // Skip '<!'
        advance(); advance();
        
        // Check if it's DOCTYPE
        std::string check;
        for (int i = 0; i < 7 && pos + i < htmlContent.size(); i++) {
            check += htmlContent[pos + i];
        }
        
        if (toLower(check.substr(0, 7)) == "doctype") {
            // Skip until '>'
            while (!atEnd() && current() != '>') {
                advance();
            }
            if (!atEnd()) {
                advance(); // Skip '>'
            }
        }
    }
    
    // Parse attributes from a tag
    std::map<std::string, std::string> parseAttributes() {
        std::map<std::string, std::string> attrs;
        
        while (!atEnd()) {
            skipWhitespace();
            
            // Check for end of tag
            if (current() == '>' || current() == '/') {
                break;
            }
            
            // Parse attribute name
            std::string name;
            while (!atEnd() && !std::isspace(static_cast<unsigned char>(current())) 
                   && current() != '>' && current() != '/' && current() != '=') {
                name += current();
                advance();
            }
            
            if (name.empty()) {
                break;
            }
            
            skipWhitespace();
            
            // Check for = sign
            if (current() == '=') {
                advance();
                skipWhitespace();
                
                // Parse attribute value
                std::string value;
                char quote = current();
                
                if (quote == '"' || quote == '\'') {
                    advance(); // Skip opening quote
                    while (!atEnd() && current() != quote) {
                        value += current();
                        advance();
                    }
                    if (!atEnd()) {
                        advance(); // Skip closing quote
                    }
                } else {
                    // Unquoted value
                    while (!atEnd() && !std::isspace(static_cast<unsigned char>(current())) 
                           && current() != '>' && current() != '/') {
                        value += current();
                        advance();
                    }
                }
                
                attrs[toLower(name)] = value;
            } else {
                // Boolean attribute
                attrs[toLower(name)] = "true";
            }
        }
        
        return attrs;
    }
    
    // Parse a comment (<!-- -->)
    NodePtr parseComment() {
        std::string comment;
        
        // Skip '<!--'
        advance(); advance(); advance(); advance();
        
        while (!atEnd()) {
            if (current() == '-' && pos + 1 < htmlContent.size() && htmlContent[pos + 1] == '-') {
                advance(); advance(); // Skip '--'
                if (current() == '>') {
                    advance(); // Skip '>'
                }
                break;
            }
            comment += current();
            advance();
        }
        
        return std::make_shared<CommentNode>(trim(comment));
    }
    
    // Parse an element
    NodePtr parseElement() {
        if (current() != '<') {
            return nullptr;
        }
        
        advance(); // Skip '<'
        
        // Check for DOCTYPE
        if (current() == '!') {
            // Check if it's DOCTYPE
            std::string check;
            for (int i = 0; i < 7 && pos + i < htmlContent.size(); i++) {
                check += htmlContent[pos + i];
            }
            
            if (toLower(check.substr(0, 7)) == "doctype") {
                // Skip DOCTYPE and return nullptr
                skipDoctype();
                return nullptr;
            }
            
            // Check for comment <!--
            if (pos + 2 < htmlContent.size() && htmlContent[pos] == '-' && htmlContent[pos + 1] == '-') {
                return parseComment();
            }
            
            // Other <! declarations - skip them
            while (!atEnd() && current() != '>') {
                advance();
            }
            if (!atEnd()) {
                advance(); // Skip '>'
            }
            return nullptr;
        }
        
        // Check for closing tag
        if (current() == '/') {
            return nullptr; // This is a closing tag, handled by parent
        }
        
        // Parse tag name
        std::string tagName;
        while (!atEnd() && !std::isspace(static_cast<unsigned char>(current())) 
               && current() != '>' && current() != '/') {
            tagName += current();
            advance();
        }
        
        if (tagName.empty()) {
            parseErrors.push_back("Empty tag name at position " + std::to_string(pos));
            return nullptr;
        }
        
        // Parse attributes
        auto attributes = parseAttributes();
        
        // Create element node
        auto element = std::make_shared<ElementNode>(toLower(tagName));
        for (const auto& attr : attributes) {
            element->setAttribute(attr.first, attr.second);
        }
        
        // Check for self-closing tag
        bool selfClosing = false;
        if (current() == '/') {
            selfClosing = true;
            advance();
        }
        
        if (current() == '>') {
            advance();
        }
        
        // Handle void elements
        if (std::find(SELF_CLOSING_ELEMENTS.begin(), SELF_CLOSING_ELEMENTS.end(), toLower(tagName)) 
            != SELF_CLOSING_ELEMENTS.end()) {
            return element;
        }
        
        // Parse children
        while (!atEnd()) {
            skipWhitespace();
            
            // Check for closing tag
            if (current() == '<' && pos + 1 < htmlContent.size() && htmlContent[pos + 1] == '/') {
                advance(); advance(); // Skip '</'
                
                // Skip closing tag name
                while (!atEnd() && !std::isspace(static_cast<unsigned char>(current())) && current() != '>') {
                    advance();
                }
                
                if (current() == '>') {
                    advance();
                }
                
                break;
            }
            
            // Parse child node
            if (current() == '<') {
                NodePtr child = parseElement();
                if (child) {
                    element->addChild(child);
                }
            } else {
                // Parse text content
                std::string text;
                while (!atEnd() && current() != '<') {
                    text += current();
                    advance();
                }
                
                std::string trimmedText = trim(text);
                if (!trimmedText.empty()) {
                    element->addChild(std::make_shared<TextNode>(trimmedText));
                }
            }
        }
        
        return element;
    }
    
public:
    HTMLParser() : pos(0) {}
    
    // Load HTML from file
    bool loadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file: " << filepath << std::endl;
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        htmlContent = buffer.str();
        file.close();
        
        return true;
    }
    
    // Load HTML from string
    void loadFromString(const std::string& content) {
        htmlContent = content;
    }
    
    // Parse the HTML content
    NodePtr parse() {
        pos = 0;
        parseErrors.clear();
        
        std::vector<NodePtr> rootChildren;
        
        while (!atEnd()) {
            skipWhitespace();
            
            if (atEnd()) break;
            
            if (current() == '<') {
                // Check for DOCTYPE
                if (pos + 1 < htmlContent.size() && htmlContent[pos + 1] == '!') {
                    std::string check;
                    for (int i = 0; i < 7 && pos + 1 + i < htmlContent.size(); i++) {
                        check += htmlContent[pos + 1 + i];
                    }
                    
                    if (toLower(check.substr(0, 7)) == "doctype") {
                        skipDoctype();
                        continue;
                    }
                }
                
                NodePtr node = parseElement();
                if (node) {
                    rootChildren.push_back(node);
                }
            } else {
                // Text before any tag
                std::string text;
                while (!atEnd() && current() != '<') {
                    text += current();
                    advance();
                }
                
                std::string trimmedText = trim(text);
                if (!trimmedText.empty()) {
                    rootChildren.push_back(std::make_shared<TextNode>(trimmedText));
                }
            }
        }
        
        // Report any parse errors
        if (!parseErrors.empty()) {
            std::cerr << "Parse warnings:" << std::endl;
            for (const auto& error : parseErrors) {
                std::cerr << "  - " << error << std::endl;
            }
        }
        
        // If we have a single root element, return it
        // Otherwise, wrap children in a document-like element
        if (rootChildren.size() == 1) {
            return rootChildren[0];
        } else if (!rootChildren.empty()) {
            auto document = std::make_shared<ElementNode>("document");
            for (const auto& child : rootChildren) {
                document->addChild(child);
            }
            return document;
        }
        
        return nullptr;
    }
    
    // Get parse errors
    const std::vector<std::string>& getErrors() const {
        return parseErrors;
    }
};

// Print usage information
void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " <html_file_path>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "This program reads an HTML file, parses it, and creates a DOM tree structure." << std::endl;
    std::cerr << "It then prints the tree to stdout." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string filepath = argv[1];
    
    // Create parser and load file
    HTMLParser parser;
    if (!parser.loadFromFile(filepath)) {
        return 1;
    }
    
    // Parse the HTML
    NodePtr root = parser.parse();
    
    if (!root) {
        std::cerr << "Error: Failed to parse HTML file" << std::endl;
        return 1;
    }
    
    // Print the DOM tree
    std::cout << "DOM Tree Structure:" << std::endl;
    std::cout << std::endl;
    root->print();
    
    return 0;
}
