#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cctype>

// SaferCPlusPlus includes
#include "msepointerbasics.h"
#include "mserefcounting.h"
#include "msemsevector.h"
#include "msemstdstring.h"
#include "msesuppresschecks.h"

// Forward declarations
class Node;
class ElementNode;
class TextNode;
class CommentNode;

// Type alias for node pointers - using lifetime-annotated reference counting pointer
using NodePtr = mse::rsv::TXSLTARefCountingPointer<Node>;

// Base Node class
class Node {
public:
    virtual ~Node() = default;
    
    virtual mse::mstd::string getType() const { return "Node"; }
    
    virtual void print(int indent = 0) const {
        mse::mstd::string spaces(indent * 2, ' ');
        std::cout << spaces << getType() << std::endl;
    }
    
    virtual mse::mstd::string toString() const { return ""; }
};

// TextNode class for text content
class TextNode : public Node {
private:
    mse::mstd::string text;
    
public:
    explicit TextNode(const mse::mstd::string& text) : text(text) {}
    
    mse::mstd::string getType() const override { return "TextNode"; }
    
    void print(int indent = 0) const override {
        mse::mstd::string spaces(indent * 2, ' ');
        std::cout << spaces << "TextNode: \"" << text << "\"" << std::endl;
    }
    
    mse::mstd::string getText() const { return text; }
    
    mse::mstd::string toString() const override { return text; }
};

// CommentNode class for HTML comments
class CommentNode : public Node {
private:
    mse::mstd::string comment;
    
public:
    explicit CommentNode(const mse::mstd::string& comment) : comment(comment) {}
    
    mse::mstd::string getType() const override { return "CommentNode"; }
    
    void print(int indent = 0) const override {
        mse::mstd::string spaces(indent * 2, ' ');
        std::cout << spaces << "CommentNode: \"" << comment << "\"" << std::endl;
    }
    
    mse::mstd::string getComment() const { return comment; }
    
    mse::mstd::string toString() const override { return "<!--" + comment + "-->"; }
};

// ElementNode class for HTML elements
class ElementNode : public Node {
private:
    mse::mstd::string tagName;
    MSE_SUPPRESS_CHECK_IN_DECLSCOPE std::map<mse::mstd::string, mse::mstd::string> attributes;
    mse::rsv::xslta_vector<NodePtr> children_dyn;
    
public:
    explicit ElementNode(const mse::mstd::string& tagName) : tagName(tagName) {}
    
    mse::mstd::string getType() const override { return "ElementNode"; }
    
    void print(int indent = 0) const override {
        mse::mstd::string spaces(indent * 2, ' ');
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
        
        // Use accessing fixed vector for iteration
        auto children = mse::rsv::make_xslta_accessing_fixed_vector(&children_dyn);
        for (const auto& child_dyn : children) {
            // Use accessing fixed owning pointer for the smart pointer element
            auto child = mse::rsv::make_xslta_accessing_fixed_owning_pointer(&child_dyn);
            child->print(indent + 1);
        }
    }
    
    void addChild(NodePtr child) {
        children_dyn.push_back(std::move(child));
    }
    
    void setAttribute(const mse::mstd::string& name, const mse::mstd::string& value) {
        attributes[name] = value;
    }
    
    mse::mstd::string getAttribute(const mse::mstd::string& name) const {
        auto it = attributes.find(name);
        return (it != attributes.end()) ? it->second : "";
    }
    
    mse::mstd::string getTagName() const { return tagName; }
    
    mse::rsv::xslta_vector<NodePtr> getChildren() const { return children_dyn; }
    
    mse::mstd::string toString() const override {
        mse::mstd::string result = "<" + tagName;
        
        for (const auto& attr : attributes) {
            result += " " + attr.first + "=\"" + attr.second + "\"";
        }
        
        result += ">";
        
        // Use accessing fixed vector for iteration
        auto children = mse::rsv::make_xslta_accessing_fixed_vector(&children_dyn);
        for (const auto& child_dyn : children) {
            // Use accessing fixed owning pointer for the smart pointer element
            auto child = mse::rsv::make_xslta_accessing_fixed_owning_pointer(&child_dyn);
            result += child->toString();
        }
        
        result += "</" + tagName + ">";
        
        return result;
    }
};

// Self-closing elements (void elements)
const mse::rsv::xslta_vector<mse::mstd::string> SELF_CLOSING_ELEMENTS = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr"
};

// Common elements we'll focus on
const mse::rsv::xslta_vector<mse::mstd::string> COMMON_ELEMENTS = {
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
    mse::mstd::string htmlContent;
    size_t pos;
    mse::rsv::xslta_vector<mse::mstd::string> parseErrors_dyn;
    
    // Helper to trim whitespace
    mse::mstd::string trim(const mse::mstd::string& str) const {
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
    mse::mstd::string toLower(const mse::mstd::string& str) const {
        mse::mstd::string result = str;
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
        mse::mstd::string check;
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
    std::map<mse::mstd::string, mse::mstd::string> parseAttributes() {
        std::map<mse::mstd::string, mse::mstd::string> attrs;
        
        while (!atEnd()) {
            skipWhitespace();
            
            // Check for end of tag
            if (current() == '>' || current() == '/') {
                break;
            }
            
            // Parse attribute name
            mse::mstd::string name;
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
                mse::mstd::string value;
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
        mse::mstd::string comment;
        
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
        
        return mse::rsv::make_xslta_nullable_refcounting(CommentNode{trim(comment)});
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
            mse::mstd::string check;
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
        mse::mstd::string tagName;
        while (!atEnd() && !std::isspace(static_cast<unsigned char>(current())) 
               && current() != '>' && current() != '/') {
            tagName += current();
            advance();
        }
        
        if (tagName.empty()) {
            parseErrors_dyn.push_back("Empty tag name at position " + std::to_string(pos));
            return nullptr;
        }
        
        // Parse attributes
        auto attributes = parseAttributes();
        
        // Create element node
        auto element = mse::rsv::make_xslta_nullable_refcounting(ElementNode{toLower(tagName)});
        for (const auto& attr : attributes) {
            {
                auto element_fixed = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&element);
                element_fixed->setAttribute(attr.first, attr.second);
            }
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
        auto selfClosingFixed = mse::rsv::make_xslta_accessing_fixed_vector(&SELF_CLOSING_ELEMENTS);
        bool isVoidElement = false;
        for (const auto& elem : selfClosingFixed) {
            if (elem == toLower(tagName)) {
                isVoidElement = true;
                break;
            }
        }
        
        if (isVoidElement) {
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
                    {
                        auto element_fixed = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&element);
                        element_fixed->addChild(child);
                    }
                }
            } else {
                // Parse text content
                mse::mstd::string text;
                while (!atEnd() && current() != '<') {
                    text += current();
                    advance();
                }
                
                mse::mstd::string trimmedText = trim(text);
                if (!trimmedText.empty()) {
                    {
                        auto element_fixed = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&element);
                        element_fixed->addChild(mse::rsv::make_xslta_nullable_refcounting(TextNode{trimmedText}));
                    }
                }
            }
        }
        
        return element;
    }
    
public:
    HTMLParser() : pos(0) {}
    
    // Load HTML from file
    bool loadFromFile(const mse::mstd::string& filepath) {
        std::ifstream file(filepath.c_str());
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
    void loadFromString(const mse::mstd::string& content) {
        htmlContent = content;
    }
    
    // Parse the HTML content
    NodePtr parse() {
        pos = 0;
        parseErrors_dyn.clear();
        
        mse::rsv::xslta_vector<NodePtr> rootChildren;
        
        while (!atEnd()) {
            skipWhitespace();
            
            if (atEnd()) break;
            
            if (current() == '<') {
                // Check for DOCTYPE
                if (pos + 1 < htmlContent.size() && htmlContent[pos + 1] == '!') {
                    mse::mstd::string check;
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
                    rootChildren.push_back(std::move(node));
                }
            } else {
                // Text before any tag
                mse::mstd::string text;
                while (!atEnd() && current() != '<') {
                    text += current();
                    advance();
                }
                
                mse::mstd::string trimmedText = trim(text);
                if (!trimmedText.empty()) {
                    rootChildren.push_back(mse::rsv::make_xslta_nullable_refcounting(TextNode{trimmedText}));
                }
            }
        }
        
        // Report any parse errors
        if (!parseErrors_dyn.empty()) {
            std::cerr << "Parse warnings:" << std::endl;
            auto errorsFixed = mse::rsv::make_xslta_accessing_fixed_vector(&parseErrors_dyn);
            for (const auto& error : errorsFixed) {
                std::cerr << "  - " << error << std::endl;
            }
        }
        
        // If we have a single root element, return it
        // Otherwise, wrap children in a document-like element
        if (rootChildren.size() == 1) {
            auto firstFixed = mse::rsv::make_xslta_accessing_fixed_vector(&rootChildren);
            return firstFixed[0];
        } else if (!rootChildren.empty()) {
            auto document = mse::rsv::make_xslta_nullable_refcounting(ElementNode{"document"});
            auto childrenFixed = mse::rsv::make_xslta_accessing_fixed_vector(&rootChildren);
            for (const auto& child_dyn : childrenFixed) {
                {
                    auto docFixed = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&document);
                    docFixed->addChild(child_dyn);
                }
            }
            return document;
        }
        
        return nullptr;
    }
    
    // Get parse errors
    mse::rsv::xslta_vector<mse::mstd::string> getErrors() const {
        return parseErrors_dyn;
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
    
    // Convert char* to mse::mstd::string properly
    MSE_SUPPRESS_CHECK_IN_XSCOPE std::string filepath_std = argv[1];
    mse::mstd::string filepath = filepath_std;
    
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
    {
        auto rootFixed = mse::rsv::make_xslta_accessing_fixed_owning_pointer(&root);
        rootFixed->print();
    }
    
    return 0;
}
