/**
 * PropsVisitor extracted from simgear props_io.cxx by David Megginson, david@megginson.com - Public Domain.
 * 
 * Changes Copyright 2021 R.J.Harrison
 * PropsVisitor is used (for us) to read in tree or subtree from an XML file
 * any node can include a sub-tree 
 * alias supported
 * arrays via attribte n="#" where # is the IDX
 * -
 * not supported the vector types (because we don't have them).
 * adpated to work with the JSBSim properties implementation (which is also based on David's original
 * props.cxx from simgear) but is a subset
 *
 * although include theoretically would work it is currently disabled until we have a way of resolving the paths
 */
#include "stdafx.h"

#include <cstdlib>    //    size_t
#include <string>
#include <sstream>
#include <iomanip>
#include <input_output/FGXMLFileRead.h>
#include "exception.hxx"
#include <algorithm>

#include "../JSBSim/src/input_output/FGXMLParse.h"
#include <input_output\FGPropertyManager.h>

const int DEFAULT_MODE = 0;// (SGPropertyNode::READ | SGPropertyNode::WRITE);
const std::string ATTR = "_attr_";
class PropsVisitor : public JSBSim::FGXMLParse
{
public:

    PropsVisitor(SGPropertyNode *root, const std::string& base, int default_mode = 0, bool extended = false)
        : _default_mode(default_mode), _root(root), _level(0), _base(base),
        _hasException(false), _extended(extended)
    {}

    virtual ~PropsVisitor() {}

    void startXML() {
        _level = 0;
        _state_stack.resize(0);
    }
    void endXML() {
        _level = 0;
        _state_stack.resize(0);
    }

    void startElement(const char* name, const XMLAttributes& atts) {
        const char* attval;
        const sg_location location(getPath(), getLine(), getColumn());

        if (_level == 0) {
            if (strcmp(name, "PropertyList")) {
                string message = "Root element name is ";
                message += name;
                message += "; expected PropertyList";
                throw sg_io_exception(message, location);
            }

            // Check for an include.
            attval = atts.getValue("include");
            if (attval != 0) {
                try {
                    //TODO: search for file in path...
                    //SGPath path = simgear::ResourceManager::instance()->findPath(attval, SGPath(_base).dir());
                    //if (path.isNull())
                    //{
                        //string message = "Cannot open file ";
                        //message = "Find file logic missing";
                        //message += attval;
                        //throw sg_io_exception(message, location);
                    //}
                    //readProperties(path, _root, 0, _extended);

                    // for now just assume that the filename will be found at the location in the attribute value.
                    readProperties(attval, _root, 0, _extended);
                        
                }
                catch (sg_io_exception & e) {
                    setException(e);
                }
            }

            push_state(_root, "", DEFAULT_MODE);
        }

        else {
            State& st = state();

            // Get the index.
            attval = atts.getValue("n");
            int index = 0;
            string strName(name);
            if (attval != 0) {
                index = atoi(attval);
                st.counters[strName] = std::max(st.counters[strName], index + 1);
            }
            else {
                index = st.counters[strName];
                st.counters[strName]++;
            }

            // Got the index, so grab the node.
            SGPropertyNode* node = st.node->getChild(strName, index, true);
            if (!node->getAttribute(SGPropertyNode::WRITE)) {
                SG_LOG(SG_INPUT, SG_ALERT, "Not overwriting write-protected property "
                    << node->getPath(true) << "\n at " << location.asString());
                node = &null;
            }

            // TODO use correct default mode (keep for now to match past behavior)
            int mode = _default_mode | SGPropertyNode::READ | SGPropertyNode::WRITE;
            int omit = false;
            const char* type = 0;

            SGPropertyNode* attr_node = NULL;

            for (int i = 0; i < atts.size(); ++i)
            {
                const std::string att_name = atts.getName(i);
                const std::string val = atts.getValue(i);

                // Get the access-mode attributes,
                // but don't set yet (in case they
                // prevent us from recording the value).
                if (att_name == "read")
                    setFlag(mode, SGPropertyNode::READ, val, location);
                else if (att_name == "write")
                    setFlag(mode, SGPropertyNode::WRITE, val, location);
                else if (att_name == "archive")
                    setFlag(mode, SGPropertyNode::ARCHIVE, val, location);
                else if (att_name == "trace-read")
                    setFlag(mode, SGPropertyNode::TRACE_READ, val, location);
                else if (att_name == "trace-write")
                    setFlag(mode, SGPropertyNode::TRACE_WRITE, val, location);
                else if (att_name == "userarchive")
                    setFlag(mode, SGPropertyNode::USERARCHIVE, val, location);
                else if (att_name == "preserve")
                    setFlag(mode, SGPropertyNode::PRESERVE, val, location);
                // note we intentionally don't handle PROTECTED here, it's
                // designed to be only set from compiled code, not loaded
                // dynamically.

                // Check for an alias.
                else if (att_name == "alias")
                {
                    if (!node->alias(val))
                        SG_LOG
                        (
                            SG_INPUT,
                            SG_ALERT,
                            "Failed to set alias to " << val << "\n at " << location.asString()
                        );
                }

                // Check for an include.
                else if (att_name == "include")
                {
                    try
                    {
                        /*SGPath path = simgear::ResourceManager::instance()->findPath(val, SGPath(_base).dir());
                        if (path.isNull())
                        {*/

//string message = "Cannot open file ";
//message += val;
//throw sg_io_exception(message, location);
                       /* }
                        readProperties(path, node, 0, _extended);*/
                        readProperties(val, _root, 0, _extended);

                    }
                    catch (sg_io_exception & e)
                    {
                        setException(e);
                    }
                }
                else if (att_name == "omit-node")
                    setFlag(omit, 1, val, location);
                else if (att_name == "type")
                {
                    type = atts.getValue(i);

                    // if a type is given and the node is tied,
                    // don't clear the value because
                    // clearValue() unties the property
                    if (!node->isTied())
                        node->clearValue();
                }
                else if (att_name != "n")
                {
                    // Store all additional attributes in a special node named _attr_
                    if (!attr_node)
                        attr_node = node->getChild(ATTR, 0, true);

                    attr_node->setUnspecifiedValue(att_name.c_str(), val.c_str());
                }
            }
            push_state(node, type, mode, omit);
        }
    }
    void endElement(const char* name) {
        State& st = state();
        bool ret;
        const sg_location location(getPath(), getLine(), getColumn());

        // If there are no children and it's
        // not an alias, then it's a leaf value.
        if (!st.hasChildren() && !st.node->isAlias())
        {
            if (st.type == "bool") {
                if (_data == "true" || atoi(_data.c_str()) != 0)
                    ret = st.node->setBoolValue(true);
                else
                    ret = st.node->setBoolValue(false);
            }
            else if (st.type == "int") {
                ret = st.node->setIntValue(atoi(_data.c_str()));
            }
            else if (st.type == "long") {
                ret = st.node->setLongValue(strtol(_data.c_str(), 0, 0));
            }
            else if (st.type == "float") {
                ret = st.node->setFloatValue(atof(_data.c_str()));
            }
            else if (st.type == "double") {
                ret = st.node->setDoubleValue(strtod(_data.c_str(), 0));
            }
            else if (st.type == "string") {
                ret = st.node->setStringValue(_data.c_str());
            }
            /*else if (st.type == "vec3d" && _extended) {
                ret = st.node
                    ->setValue(simgear::parseString<SGVec3d>(_data));
            }
            else if (st.type == "vec4d" && _extended) {
                ret = st.node
                    ->setValue(simgear::parseString<SGVec4d>(_data));
            }*/
            else if (st.type == "unspecified") {
                ret = st.node->setUnspecifiedValue(_data.c_str());
            }
            else if (_level == 1) {
                ret = true;		// empty <PropertyList>
            }
            else {
                string message = "Unrecognized data type '";
                message += st.type;
                message += '\'';
                throw sg_io_exception(message, location);
            }
            if (!ret)
                SG_LOG
                (
                    SG_INPUT,
                    SG_ALERT,
                    "readProperties: Failed to set " << st.node->getPath()
                    << " to value \"" << _data
                    << "\" with type " << st.type
                    << "\n at " << location.asString()
                );
        }

        // Set the access-mode attributes now,
        // once the value has already been
        // assigned.
        st.node->setAttributes(st.mode);

        if (st.omit) {
            State& parent = _state_stack[_state_stack.size() - 2];
            int nChildren = st.node->nChildren();
            for (int i = 0; i < nChildren; i++) {
                SGPropertyNode* src = st.node->getChild(i);
                std::string name = src->getNameString();
                int index = parent.counters[name];
                parent.counters[name]++;
                SGPropertyNode* dst = parent.node->getChild(name, index, true);
                copyProperties(src, dst);
            }
            parent.node->removeChild(st.node->getNameString(), st.node->getIndex());
        }
        pop_state();
    }
    void data(const char* s, int length) {
        if (!state().hasChildren())
            _data.append(std::string(s, length));
    }

    void warning(const char* message, int line, int column) {
        SG_LOG(SG_INPUT, SG_ALERT, "readProperties: warning: "
            << message << " at line " << line << ", column " << column);
    }

    bool hasException() const { return _hasException; }
    sg_io_exception& getException() { return _exception; }
    void setException(const sg_io_exception& exception) {
        _exception = exception;
        _hasException = true;
    }

private:
    /**
 * Set/unset a yes/no flag.
 */
    static void
        setFlag(int& mode,
            int mask,
            const std::string& flag,
            const sg_location& location)
    {
        if (flag == "y")
            mode |= mask;
        else if (flag == "n")
            mode &= ~mask;
        else
        {
            string message = "Unrecognized flag value '";
            message += flag;
            message += '\'';
            throw sg_io_exception(message, location);
        }
    }

    bool
        copyPropertyValue(const SGPropertyNode* in, SGPropertyNode* out)
    {
        using namespace simgear;
        bool retval = true;

        if (!in->hasValue()) {
            return true;
        }

        switch (in->getType()) {
        case props::BOOL:
            if (!out->setBoolValue(in->getBoolValue()))
                retval = false;
            break;
        case props::INT:
            if (!out->setIntValue(in->getIntValue()))
                retval = false;
            break;
        case props::LONG:
            if (!out->setLongValue(in->getLongValue()))
                retval = false;
            break;
        case props::FLOAT:
            if (!out->setFloatValue(in->getFloatValue()))
                retval = false;
            break;
        case props::DOUBLE:
            if (!out->setDoubleValue(in->getDoubleValue()))
                retval = false;
            break;
        case props::STRING:
            if (!out->setStringValue(in->getStringValue()))
                retval = false;
            break;
        case props::UNSPECIFIED:
            if (!out->setUnspecifiedValue(in->getStringValue()))
                retval = false;
            break;
      /*  case props::VEC3D:
            if (!out->setValue(in->getValue<SGVec3d>()))
                retval = false;
            break;
        case props::VEC4D:
            if (!out->setValue(in->getValue<SGVec4d>()))
                retval = false;
            break;*/
        default:
            if (in->isAlias())
                break;
            string message = "Unknown internal SGPropertyNode type";
            message += in->getType();
            throw sg_error(message);
        }

        return retval;
    }
    /**
     * Copy one property tree to another.
     *
     * @param in The source property tree.
     * @param out The destination property tree.
     * @return true if all properties were copied, false if some failed
     *  (for example, if the property's value is tied read-only).
     */
    bool
        copyProperties(const SGPropertyNode* in, SGPropertyNode* out)
    {
        using namespace simgear;
        bool retval = copyPropertyValue(in, out);
        if (!retval) {
            return false;
        }

        // copy the attributes.
        out->setAttributes(in->getAttributes());

        // Next, copy the children.
        int nChildren = in->nChildren();
        for (int i = 0; i < nChildren; i++) {
            const SGPropertyNode* in_child = in->getChild(i);
            SGPropertyNode* out_child = out->getChild(in_child->getNameString(),
                in_child->getIndex(),
                false);
            if (!out_child)
            {
                out_child = out->getChild(in_child->getNameString(),
                    in_child->getIndex(),
                    true);
            }

            if (out_child && !copyProperties(in_child, out_child))
                retval = false;
        }

        return retval;
    }
    /**
 * Read properties from a file.
 *
 * @param file A string containing the file path.
 * @param start_node The root node for reading properties.
 * @return true if the read succeeded, false otherwise.
 */
    void readProperties(const std::string &file, SGPropertyNode* start_node,
            int default_mode, bool extended)
    {
        PropsVisitor visitor(start_node, file.c_str(), default_mode, extended);
        readXML(file, visitor);
        if (visitor.hasException())
            throw visitor.getException();
    }


    struct State
    {
        State() : node(0), type(""), mode(DEFAULT_MODE), omit(false) {}
        State(SGPropertyNode* _node, const char* _type, int _mode, bool _omit)
            : node(_node), type(_type), mode(_mode), omit(_omit) {}
        bool hasChildren() const
        {
            int n_children = node->nChildren();
            return n_children > 1
                || (n_children == 1 && node->getChild(0)->getNameString() != ATTR);
        }
        SGPropertyNode* node;
        string type;
        int mode;
        bool omit;
        map<string, int> counters;
    };

    State& state() { return _state_stack[_state_stack.size() - 1]; }

    void push_state(SGPropertyNode* node, const char* type, int mode, bool omit = false) {
        if (type == 0)
            _state_stack.push_back(State(node, "unspecified", mode, omit));
        else
            _state_stack.push_back(State(node, type, mode, omit));
        _level++;
        _data = "";
    }

    void pop_state() {
        _state_stack.pop_back();
        _level--;
    }
    int _default_mode;
    std::string _data;
    int _level;
    SGPropertyNode* _root;
    SGPropertyNode null;

    std::vector<State> _state_stack;
    std::string _base;
    sg_io_exception _exception;
    bool _hasException;
    bool _extended;
};


/**
 * Set/unset a yes/no flag.
 */
static void
setFlag(int& mode,
    int mask,
    const std::string& flag,
    const sg_location& location)
{
    if (flag == "y")
        mode |= mask;
    else if (flag == "n")
        mode &= ~mask;
    else
    {
        std::string message = "Unrecognized flag value '";
        message += flag;
        message += '\'';
        throw sg_io_exception(message, location);
    }
}
