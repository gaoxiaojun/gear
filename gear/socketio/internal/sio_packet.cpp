//
//  sio_packet.cpp
//
//  Created by Melo Yao on 3/22/15.
//

#include "sio_packet.h"
#include "json.hpp"
#include <cassert>

#define kBIN_PLACE_HOLDER "_placeholder"

namespace sio {
using json = nlohmann::json;
using namespace std;
void accept_message(message const& msg, json& j, vector<shared_ptr<const string> >& buffers);
void accept_object_message(object_message const& msg, json& jObject,
                           vector<shared_ptr<const string> >& buffers);
void accept_array_message(array_message const& msg, json& jArray,
                          vector<shared_ptr<const string> >& buffers);

void accept_binary_message(binary_message const& msg, json& j,
                           vector<shared_ptr<const string> >& buffers) {
  j[kBIN_PLACE_HOLDER] = true;
  j["num"] = (int)buffers.size();
  // FIXME can not avoid binary copy here.
  shared_ptr<string> write_buffer = make_shared<string>();
  write_buffer->reserve(msg.get_binary()->size() + 1);
  char frame_char = packet::frame_message;
  write_buffer->append(&frame_char, 1);
  write_buffer->append(*(msg.get_binary()));
  buffers.push_back(write_buffer);
}

void accept_array_element_message(message const& msg, json& j,
                                  vector<shared_ptr<const string> >& buffers) {
  const message* msg_ptr = &msg;
  switch (msg.get_flag()) {
    case message::flag_integer: {
      j.push_back((*(static_cast<const int_message*>(msg_ptr))).get_int());
      break;
    }
    case message::flag_double: {
      j.push_back((*(static_cast<const double_message*>(msg_ptr))).get_double());
      break;
    }
    case message::flag_string: {
      j.push_back((*(static_cast<const string_message*>(msg_ptr))).get_string());
      break;
    }
    case message::flag_boolean: {
      j.push_back((*(static_cast<const bool_message*>(msg_ptr))).get_bool());
      break;
    }
    case message::flag_object: {
      json jChild = json::object();
      accept_object_message((*(static_cast<const object_message*>(msg_ptr))), jChild, buffers);
      j.push_back(jChild);
      break;
    }
    case message::flag_array: {
      json jChild = json::array();
      accept_array_message((*(static_cast<const array_message*>(msg_ptr))), jChild, buffers);
      j.push_back(jChild);
      break;
    }
    case message::flag_null: {
      j.push_back(nullptr);
      break;
    }
    default:
      break;
  }
}

void accept_primitive_element_message(message const& msg, json& j, string const& key) {
  const message* msg_ptr = &msg;
  switch (msg.get_flag()) {
    case message::flag_integer: {
      j[key] = (*(static_cast<const int_message*>(msg_ptr))).get_int();
      break;
    }
    case message::flag_double: {
      j[key] = (*(static_cast<const double_message*>(msg_ptr))).get_double();
      break;
    }
    case message::flag_string: {
      j[key] = (*(static_cast<const string_message*>(msg_ptr))).get_string();
      break;
    }
    case message::flag_boolean: {
      j[key] = (*(static_cast<const bool_message*>(msg_ptr))).get_bool();
      break;
    }
    case message::flag_null: {
      j[key] = nullptr;
      break;
    }
    default:
      break;
  }
}

void accept_array_message(array_message const& msg, json& jArray,
                          vector<shared_ptr<const string> >& buffers) {
  for (vector<message::ptr>::const_iterator it = msg.get_vector().begin();
       it != msg.get_vector().end(); ++it) {
    accept_array_element_message(*(*it), jArray, buffers);
  }
}

void accept_object_message(object_message const& msg, json& jObject,
                           vector<shared_ptr<const string> >& buffers) {
  for (map<string, message::ptr>::const_iterator it = msg.get_map().begin();
       it != msg.get_map().end(); ++it) {
    const message msg = *(it->second);
    if (msg.get_flag() == message::flag_object) {
      json jChildObject = json::object();
      accept_object_message(*(static_cast<const object_message*>(&msg)), jChildObject, buffers);
      jObject[it->first] = jChildObject;
    } else if (msg.get_flag() == message::flag_binary) {
      json jChildObject = json::object();
      accept_binary_message(*(static_cast<const binary_message*>(&msg)), jChildObject, buffers);
      jObject[it->first] = jChildObject;
    } else if (msg.get_flag() == message::flag_array) {
      json jChildArray = json::array();
      accept_array_message(*(static_cast<const array_message*>(&msg)), jChildArray, buffers);
      jObject[it->first] = jChildArray;
    } else {
      accept_primitive_element_message(msg, jObject, it->first);
    }
  }
}

void accept_message(message const& msg, json& j, vector<shared_ptr<const string> >& buffers) {
  const message* msg_ptr = &msg;
  switch (msg.get_flag()) {
    case message::flag_binary: {
      j = json::object();
      accept_binary_message(*(static_cast<const binary_message*>(msg_ptr)), j, buffers);
      break;
    }
    case message::flag_array: {
      j = json::array();
      accept_array_message(*(static_cast<const array_message*>(msg_ptr)), j, buffers);
      break;
    }
    case message::flag_object: {
      j = json::object();
      accept_object_message(*(static_cast<const object_message*>(msg_ptr)), j, buffers);
      break;
    }
    default:
      break;
  }
}

message::ptr from_json(const json& j, vector<shared_ptr<const string> > const& buffers) {
  if (j.is_number_integer()) {
    return int_message::create(j.get<int>());
  } else if (j.is_number_float()) {
    return double_message::create(j.get<double>());
  } else if (j.is_string()) {
    return string_message::create(j.get<std::string>());
  } else if (j.is_array()) {
    message::ptr ptr = array_message::create();
    for (auto& element : j) {
      static_cast<array_message*>(ptr.get())->get_vector().push_back(from_json(element, buffers));
    }
    return ptr;
  } else if (j.is_object()) {
    // binary placeholder
    auto placeHolder = j.find(kBIN_PLACE_HOLDER);
    if (placeHolder != j.end() && placeHolder.value()) {
      int num = j["num"];
      if (num >= 0 && num < static_cast<int>(buffers.size())) {
        return binary_message::create(buffers[num]);
      }
      return message::ptr();
    }

    // real object message.
    message::ptr ptr = object_message::create();
    for (json::const_iterator it = j.begin(); it != j.end(); ++it) {
      static_cast<object_message*>(ptr.get())->get_map()[it.key()] =
          from_json(j.at(it.key()), buffers);
    }
    return ptr;
  } else if (j.is_boolean()) {
    return bool_message::create(j.get<bool>());
  } else if (j.is_null()) {
    return null_message::create();
  }
  return message::ptr();
}

packet::packet(string const& nsp, message::ptr const& msg, int pack_id, bool isAck)
    : _frame(frame_message),
      _type((isAck ? type_ack : type_event) | type_undetermined),
      _nsp(nsp),
      _message(msg),
      _pack_id(pack_id),
      _pending_buffers(0) {
  assert((!isAck || (isAck && pack_id >= 0)));
}

packet::packet(type type, string const& nsp, message::ptr const& msg)
    : _frame(frame_message),
      _type(type),
      _nsp(nsp),
      _message(msg),
      _pack_id(-1),
      _pending_buffers(0) {}

packet::packet(packet::frame_type frame)
    : _frame(frame), _type(type_undetermined), _pack_id(-1), _pending_buffers(0) {}

packet::packet() : _type(type_undetermined), _pack_id(-1), _pending_buffers(0) {}

bool packet::is_binary_message(string const& payload_ptr) {
  return payload_ptr.size() > 0 && payload_ptr[0] == frame_message;
}

bool packet::is_text_message(string const& payload_ptr) {
  return payload_ptr.size() > 0 && payload_ptr[0] == (frame_message + '0');
}

bool packet::is_message(string const& payload_ptr) {
  return is_binary_message(payload_ptr) || is_text_message(payload_ptr);
}

bool packet::parse_buffer(const string& buf_payload) {
  if (_pending_buffers > 0) {
    assert(is_binary_message(buf_payload));  // this is ensured by outside.
    _buffers.push_back(std::make_shared<string>(buf_payload.data() + 1, buf_payload.size() - 1));
    _pending_buffers--;
    if (_pending_buffers == 0) {
      auto json = json::parse(_buffers.front()->data());
      _buffers.erase(_buffers.begin());
      _message = from_json(json, _buffers);
      _buffers.clear();
      return false;
    }
    return true;
  }
  return false;
}

bool packet::parse(const string& payload_ptr) {
  assert(!is_binary_message(payload_ptr));  // this is ensured by outside
  _frame = (packet::frame_type)(payload_ptr[0] - '0');
  _message.reset();
  _pack_id = -1;
  _buffers.clear();
  _pending_buffers = 0;
  size_t pos = 1;
  if (_frame == frame_message) {
    _type = (packet::type)(payload_ptr[pos] - '0');
    if (_type < type_min || _type > type_max) {
      return false;
    }
    pos++;
    if (_type == type_binary_event || _type == type_binary_ack) {
      size_t score_pos = payload_ptr.find('-');
      _pending_buffers = static_cast<unsigned int>(std::stoi(payload_ptr.substr(pos, score_pos)));
      pos = score_pos + 1;
    }
  }

  size_t nsp_json_pos = payload_ptr.find_first_of("{[\"/", pos, 4);
  if (nsp_json_pos == string::npos)  // no namespace and no message,the end.
  {
    _nsp = "/";
    return false;
  }
  size_t json_pos = nsp_json_pos;
  if (payload_ptr[nsp_json_pos] == '/')  // nsp_json_pos is start of nsp
  {
    size_t comma_pos = payload_ptr.find_first_of(",");  // end of nsp
    if (comma_pos == string::npos)                      // packet end with nsp
    {
      _nsp = payload_ptr.substr(nsp_json_pos);
      return false;
    } else  // we have a message, maybe the message have an id.
    {
      _nsp = payload_ptr.substr(nsp_json_pos, comma_pos - nsp_json_pos);
      pos = comma_pos + 1;                                   // start of the message
      json_pos = payload_ptr.find_first_of("\"[{", pos, 3);  // start of the json part of message
      if (json_pos == string::npos) {
        // no message,the end
        // assume if there's no message, there's no message id.
        return false;
      }
    }
  } else {
    _nsp = "/";
  }

  if (pos < json_pos)  // we've got pack id.
  {
    _pack_id = std::stoi(payload_ptr.substr(pos, json_pos - pos));
  }
  if (_frame == frame_message && (_type == type_binary_event || _type == type_binary_ack)) {
    // parse later when all buffers are arrived.
    _buffers.push_back(
        make_shared<string>(payload_ptr.data() + json_pos, payload_ptr.length() - json_pos));
    return true;
  } else {
    // auto j = json::parse(payload_ptr.data() + json_pos);
    auto j = json::parse(payload_ptr.substr(json_pos));
    _message = from_json(j, vector<shared_ptr<const string> >());
    return false;
  }
}

bool packet::accept(string& payload_ptr, vector<shared_ptr<const string> >& buffers) {
  char frame_char = _frame + '0';
  payload_ptr.append(&frame_char, 1);
  if (_frame != frame_message) {
    return false;
  }
  bool hasMessage = false;
  json j;
  if (_message) {
    accept_message(*_message, j, buffers);
    // j = json::parse(*(_message));
    hasMessage = true;
  }
  bool hasBinary = buffers.size() > 0;
  _type = _type & (~type_undetermined);
  if (_type == type_event) {
    _type = hasBinary ? type_binary_event : type_event;
  } else if (_type == type_ack) {
    _type = hasBinary ? type_binary_ack : type_ack;
  }
  ostringstream ss;
  ss.precision(8);
  ss << _type;
  if (hasBinary) {
    ss << buffers.size() << "-";
  }
  if (_nsp.size() > 0 && _nsp != "/") {
    ss << _nsp;
    if (hasMessage || _pack_id >= 0) {
      ss << ",";
    }
  }

  if (_pack_id >= 0) {
    ss << _pack_id;
  }

  payload_ptr.append(ss.str());
  if (hasMessage) {
    std::string jsonStr = j.dump();
    payload_ptr.append(jsonStr);
  }
  return hasBinary;
}

packet::frame_type packet::get_frame() const { return _frame; }

packet::type packet::get_type() const {
  assert((_type & type_undetermined) == 0);
  return (type)_type;
}

string const& packet::get_nsp() const { return _nsp; }

message::ptr const& packet::get_message() const { return _message; }

unsigned packet::get_pack_id() const { return _pack_id; }

void packet_manager::set_decode_callback(function<void(packet const&)> const& decode_callback) {
  m_decode_callback = decode_callback;
}

void packet_manager::set_encode_callback(
    function<void(bool, shared_ptr<const string> const&)> const& encode_callback) {
  m_encode_callback = encode_callback;
}

void packet_manager::reset() { m_partial_packet.reset(); }

void packet_manager::encode(packet& pack,
                            encode_callback_function const& override_encode_callback) const {
  shared_ptr<string> ptr = make_shared<string>();
  vector<shared_ptr<const string> > buffers;
  const encode_callback_function* cb_ptr = &m_encode_callback;
  if (override_encode_callback) {
    cb_ptr = &override_encode_callback;
  }
  if (pack.accept(*ptr, buffers)) {
    if ((*cb_ptr)) {
      (*cb_ptr)(false, ptr);
    }
    for (auto it = buffers.begin(); it != buffers.end(); ++it) {
      if ((*cb_ptr)) {
        (*cb_ptr)(true, *it);
      }
    }
  } else {
    if ((*cb_ptr)) {
      (*cb_ptr)(false, ptr);
    }
  }
}

void packet_manager::put_payload(string const& payload) {
  unique_ptr<packet> p;
  do {
    if (packet::is_text_message(payload)) {
      p.reset(new packet());
      if (p->parse(payload)) {
        m_partial_packet = std::move(p);
      } else {
        break;
      }
    } else if (packet::is_binary_message(payload)) {
      if (m_partial_packet) {
        if (!m_partial_packet->parse_buffer(payload)) {
          p = std::move(m_partial_packet);
          break;
        }
      }
    } else {
      p.reset(new packet());
      p->parse(payload);
      break;
    }
    return;
  } while (0);

  if (m_decode_callback) {
    m_decode_callback(*p);
  }
}
}
