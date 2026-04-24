Apr 2026

### AI conversion of a simple html parser the scpptool-enforced safe subset of C++

Here we present a summary of our experience (in Apr 2026) getting a local AI agent to convert an (AI generated) "toy" html parser to the scpptool-enforced safe subset. The before and after versions of the parser are provided in this repo. They are each just one file of about 600 lines, so they're fairly easy to inspect and compare. (The changes are modest enough that you can literally just `diff` the two files to see what the conversion involved.)

The local AI model we used was Qwen3.5-27B. While widely regarded an impressive advancement for models of its size, its capabilities are still clearly limited in comparison to skilled biological programmers. Fortunately the task of migrating modern C++ code to the scpptool-enforced safe subset is a fairly straightforward one and, at least in this case, with adequate prompting, within the abilities of this model.

The first thing we did was ask it to summarize (our local copy of) the scpptool repo, which it seemed to do pretty well. Then we asked it to create a plan to convert the html parser to the scpptool-enforced safe subset. There were some issues with the plan it came up with. First was that it was simply misspelling the name of some of the SaferCPlusPlus library elements. So we had it correct those. It also didn't seem to realize that `mse::make_refcounting()` returns a reference-counting pointer that does not support null values. We recommended that it use `mse::rsv::make_xslta_nullable_refcounting()` instead. In response to its updated plan we pointed out another issue:

> In the plan you give an example replacement line of code:
> 
>    auto element = mse::rsv::make_xslta_nullable_refcounting(new ElementNode(toLower(tagName)));
> 
> Neither `std::make_shared()` nor `mse::rsv::make_xslta_nullable_refcounting()` take a raw pointer to a manually allocated object. `std::make_shared()` takes as arguments the constructor arguments of its template argument type. `mse::rsv::make_xslta_nullable_refcounting()` is a little different in that it takes an already constructed (temporary) object. So the correct version of the example replacement line of code would be more like:
> 
>    auto element = mse::rsv::make_xslta_nullable_refcounting(ElementNode{toLower(tagName)});
> 
> And similarly, your example replacement for direct construction of the reference counting pointer:
> 
>     auto element = NodePtr(new ElementNode(toLower(tagName)));
> 
> should instead be more like:
> 
>     auto element = NodePtr(ElementNode{toLower(tagName)});

We also clarified that scpptool's restriction on null pointers values applies only to raw pointers, not the smart pointers in the SaferCPlusPlus library which safely support null values. And we recommended that when assigning `argv[1]` to an `mse::mstd::string` they do an intermediate conversion to an `std::string`, prefixed with `MSE_SUPPRESS_CHECK_IN_XSCOPE` since scpptool doesn't approve of `std::string`.

While we'd consider these to be fairly minor corrections and advisories, we found that we did need to give it a more extensive reminder about the way dynamic owning pointers and containers work. And perhaps this prompt might be helpful for the uninitiated biological programmer as well. Because it's a rather lengthy prompt, we'll list it at the end of this article.

So the AI agent did a final update to its plan, then we had it proceed with the conversion. Which, as far we can tell, was basically successful. Running the scpptool analyzer on the converted code yielded one complaint about the expression `argv[0]` being technically unsafe (a somewhat unavoidable and expected issue), one complaint about a `size_t` field not having a default initializer (which, to be fair, is not a requirement expressed in the documentation), and a few complaints about the use of `std::map<>`. The AI agent correctly assessed that `std::map<>` did not yet have a safe counterpart available in the SaferCPlusPlus library and so (correctly) used a check-suppression annotation prefix when declaring the `std::map<>` field. But the scpptool analyzer would also require check-suppression annotation prefixes for any attempt to obtain or use a reference to an element in the unsafe container, not just the declaration of the container. I'm sure the AI agent could have addressed these scpptool complaints on its own had we directed it to do so. But we were a little wary about having the AI agent get too comfortable with using check-suppression annotations to quiet the scpptool analyzer. :)

This was our prompt about the way dynamic owning pointers and containers work, with examples that the uninitiated biological programmer might find helpful as well:

> One more important thing. Remember that, unlike their standard library counterparts, dereferencing and content accessing operators and methods of the library's lifetime-annotated dynamic owning pointers (like `mse::rsv::TXSLTARefCountingPointer`) and containers (like `mse::rsv::xslta_vector`) return "proxy reference (or pointer)" objects, not raw references (or pointers). These proxy reference objects have very limted functionality and basically only support being cast to the element type or being assigned a value of the element type. For many other reference operations, they won't work. For example, unlike raw references (or pointers), you cannot use the "dot" (or "arrow") operator to access a (data or function) member of the target object. 
> 
> For this reason, the preferred (and more reliable) way of accessing the owned contents of a (lifetime-annotated) dynamic owning pointer or container is via (the creation of) an associated (scoped) "borrowing fixed" object (or "accessing fixed" object in situations where you only have const access to the dynamic owning pointer or container). So for example in the following snippet of code:
> 
>         auto const sensitive_keywords = std::array<std::string, 3>{ "secret", "password", "private key" };
>         auto owned_public_info = std::make_shared<std::string>(info_str);
>         for (const auto& sensitive_keyword : sensitive_keywords) {
>             if (std::string::npos != owned_public_info->find(sensitive_keyword)) {
>                 *owned_public_info = "[redacted]";
>             }
>         }
> 
> converting the `std::make_shared()` call won't be sufficient:
> 
>         auto const sensitive_keywords = mse::rsv::xslta_array<mse::mstd::string, 3>{ "secret", "password", "private key" };
>         auto owned_public_info = mse::rsv::make_xslta_nullable_refcounting(mse::mstd::string{ info_str });
>         for (const auto& sensitive_keyword : sensitive_keywords) {
>             if (std::string::npos != owned_public_info->find(sensitive_keyword)) { // compile error
>                 
>                 /* `owned_public_info`'s arrow operator returns an (almost useless) "proxy pointer" object that cannot be 
>                 used to invoke the `find()` method */
>                 
>                 *owned_public_info = "[redacted]";
>             }
>         }
> 
> Instead one would create and use an associated "borrowing fixed" object (inside a new scope) to access the member like so: 
> 
>         auto const sensitive_keywords = mse::rsv::xslta_array<mse::mstd::string, 3>{ "secret", "password", "private key" };
>         auto owned_public_info_dyn = mse::rsv::make_xslta_nullable_refcounting(mse::mstd::string{ info_str });
>         for (const auto& sensitive_keyword : sensitive_keywords) {
>             {
>                 auto owned_public_info = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&owned_public_info_dyn);
>                 if (std::string::npos != owned_public_info->find(sensitive_keyword)) {
>                     *owned_public_info = "[redacted]";
>                 }
>             }
>         }
> 
> or
> 
>         auto const sensitive_keywords = mse::rsv::xslta_array<mse::mstd::string, 3>{ "secret", "password", "private key" };
>         auto owned_public_info_dyn = mse::rsv::make_xslta_nullable_refcounting(mse::mstd::string{ info_str });
>         {
>             auto owned_public_info = mse::rsv::make_xslta_borrowing_fixed_owning_pointer(&owned_public_info_dyn);
>             for (const auto& sensitive_keyword : sensitive_keywords) {
>                 if (std::string::npos != owned_public_info->find(sensitive_keyword)) {
>                     *owned_public_info = "[redacted]";
>                 }
>             }
>         }
> 
> The reason we generally want to enclose the new "borrowing fixed" object and the (existing) dereferencing code in a new scope is because the lending (dynamic owning) object is not usable while the "borrowing fixed" object exists. An enclosing scope will ensure that the "borrowing fixed" object is destroyed promptly after use and thus won't interfere with any subsequent usage of the lending object. For example, a common subsequent use of lending objects is as a return value from the function that created the lending object. 
> 
> Also note that a subtle but common situation where dereferencing occurs is when iterating over containers. So for example in the following snippet:
> 
>     class Node {
>     public:
>         std::string toString() const { return m_str; }
>         std::string m_str;
>     };
>     
>     class ElementNode {
>         std::vector<Node> children;
>     
>     public:
>         std::string toString() const {
>             std::string result;
>             for (const auto& child : children) {
>                 result += child.toString();
>             }
>             return result;
>         }
>     };
> 
> converting only the `std::vector<>` type  won't be sufficient:
> 
>     class Node {
>     public:
>         mse::mstd::string toString() const { return m_str; }
>         mse::mstd::string m_str;
>     };
>     
>     class ElementNode {
>         mse::rsv::xstla_vector<Node> children;
>     
>     public:
>         mse::mstd::string toString() const {
>             mse::mstd::string result;
>             for (const auto& child : children) {
>                 result += child.toString(); // compile error
>                 
>                 /* child is a (raw) reference that is initialized with the value returned by the dereference of the `for` 
>                 loop's hidden iterator. That iterator originally returned a (raw) reference, but now returns a "proxy 
>                 reference" object, which cannot be used to invoke the child's toString() member function.    */
>             }
>             return result;
>         }
>     };
> 
> In this case one would instead create an associated "accessing fixed" object and ensure that the dereference operations are applied to the "accessing fixed" object rather than the dynamic container (in this case, the vector). (We use an "accessing fixed" object rather than a "borrowing fixed" object in this case because creating a "borrowing fixed" object requires a non-const pointer to the "lending" object, but in this case we only have const access to the dynamic container. "Accessing fixed" objects can be a little less efficient than "borrowing fixed" objects, but essentially serve the same purpose.): 
> 
>     class Node {
>     public:
>         mse::mstd::string toString() const { return m_str; }
>         mse::mstd::string m_str;
>     };
>     
>     class ElementNode {
>         mse::rsv::xstla_vector<Node> children_dyn;
>     
>     public:
>         mse::mstd::string toString() const {
>             mse::mstd::string result;
>             
>             auto children = mse::rsv::make_xslta_accessing_fixed_vector(&children_dyn);
>             
>             for (const auto& child : children) {
>                 result += child.toString(); // this will work now
>             }
>             return result;
>         }
>     };
> 
> Note that unlike with "borrowing fixed" objects, the lending object remains usable (as a const object) while the "accessing fixed" object exists. So unlike with "borrowing fixed" objects, the creation of a new scope is usually not necessary.
> 
> And one more situation where it's easy to miss conversion steps is the one where you have a dynamic owning pointer or container which itself owns another type of dynamic owning pointer or container. So changing the previous example slightly:
> 
>     class Node {
>     public:
>         std::string toString() const { return m_str; }
>         std::string m_str;
>     };
>     
>     using NodePtr = std::shared_ptr<Node>;
>     
>     class ElementNode {
>         std::vector<NodePtr> children;
>     
>     public:
>         std::string toString() const {
>             std::string result;
>             for (const auto& child : children) {
>                 result += child->toString();
>             }
>             return result;
>         }
>     };
> 
> In the conversion it would be easy to properly use an "accessing fixed" vector to access the vector elements, but then forget to use an "accessing fixed" owning pointer to dereference the reference counting pointer element.:
> 
>     class Node {
>     public:
>         mse::mstd::string toString() const { return m_str; }
>         mse::mstd::string m_str;
>     };
>     
>     using NodePtr = mse::rsv::TXSLTARefCountingPointer<Node>;
>     
>     class ElementNode {
>         mse::rsv::xstla_vector<NodePtr> children_dyn;
>     
>     public:
>         mse::mstd::string toString() const {
>             mse::mstd::string result;
>             
>             auto children = mse::rsv::make_xslta_accessing_fixed_vector(&children_dyn);
>             
>             for (const auto& child : children) {
>                 result += child->toString(); // still a compile error
>                 
>                 /* child is a dynamic owning pointer, so its arrow operator returns a "proxy reference" pointer which cannot 
>                 be used to invoke a member function. */
>             }
>             return result;
>         }
>     };
> 
> Because there are two nested dynamic owning entities, it can be easy to make an erroneous assumption about which one is causing the error. Something to keep in mind when dealing with nested dynamic owning pointers and containers. For completeness, here's the working version:
> 
>     class Node {
>     public:
>         mse::mstd::string toString() const { return m_str; }
>         mse::mstd::string m_str;
>     };
>     
>     using NodePtr = mse::rsv::TXSLTARefCountingPointer<Node>;
>     
>     class ElementNode {
>         mse::rsv::xstla_vector<NodePtr> children_dyn;
>     
>     public:
>         mse::mstd::string toString() const {
>             mse::mstd::string result;
>             
>             auto children = mse::rsv::make_xslta_accessing_fixed_vector(&children_dyn);
>             
>             for (const auto& child_dyn : children) {
>                 auto child = mse::rsv::make_xslta_accessing_fixed_owning_pointer(&child_dyn);
>                 result += child->toString(); // works now
>             }
>             return result;
>         }
>     };
> 
> Changing all dereferences and operations that access the owned content of the dynamic owning pointers and containers to happen via "borrowing fixed" objects or "accessing fixed" objects is the main task of this conversion job. And it's what makes the undertaking more (challenging) than just a glorified set of find-and-replace operations.

