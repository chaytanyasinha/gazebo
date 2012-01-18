/*
 * Copyright 2011 Nate Koenig & Andrew Howard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/
/* Desc: Handles pushing messages out on a named topic
 * Author: Nate Koenig
 */

#include "common/Exception.hh"
#include "transport/TopicManager.hh"
#include "transport/Publisher.hh"

using namespace gazebo;
using namespace transport;

//////////////////////////////////////////////////
// Constructor
Publisher::Publisher(unsigned int _limit, bool _latch)
{
  this->queueLimit = _limit;
  this->latch = _latch;
}

//////////////////////////////////////////////////
// Constructor
Publisher::Publisher(const std::string &_topic, const std::string &_msgType,
                     unsigned int _limit, bool _latch)
  : topic(_topic), msgType(_msgType), queueLimit(_limit), latch(_latch)
{
  this->mutex = new boost::recursive_mutex();
}

//////////////////////////////////////////////////
// Destructor
Publisher::~Publisher()
{
  if (this->messages.size() > 0)
    this->SendMessage();

  if (!this->topic.empty())
    TopicManager::Instance()->Unadvertise(this->topic);

  delete this->mutex;
}

bool Publisher::HasConnections() const
{
  return ((this->publications[0] &&
           (this->publications[0]->GetCallbackCount() > 0 ||
            this->publications[0]->GetNodeCount() > 0)) ||
          (this->publications[1] &&
           (this->publications[1]->GetCallbackCount() > 0 ||
            this->publications[0]->GetNodeCount() > 0)));
}

//////////////////////////////////////////////////
// Publish a message
void Publisher::PublishImpl(const google::protobuf::Message &_message,
                            bool /*_block*/)
{
  if (_message.GetTypeName() != this->msgType)
    gzthrow("Invalid message type\n");

  // if (!this->HasConnections())
  // return;

  // Save the latest message
  google::protobuf::Message *msg = _message.New();
  msg->CopyFrom(_message);

  if (this->latch)
  {
    msg->SerializeToString(&this->prevMsg);
  }

  this->mutex->lock();
  this->messages.push_back(msg);

  if (this->messages.size() > this->queueLimit)
  {
    delete this->messages.front();
    this->messages.pop_front();
  }
  this->mutex->unlock();
}

//////////////////////////////////////////////////
// Send the lastest message
void Publisher::SendMessage()
{
  this->mutex->lock();

  if (this->messages.size() > 0)
  {
    std::list<google::protobuf::Message *>::iterator iter;
    for (iter = this->messages.begin(); iter != this->messages.end(); ++iter)
    {
      // Send the latest message.
      TopicManager::Instance()->Publish(this->topic, **iter,
          boost::bind(&Publisher::OnPublishComplete, this));
      delete *iter;
    }

    this->messages.clear();
  }

  this->mutex->unlock();
}

unsigned int Publisher::GetOutgoingCount() const
{
  this->mutex->lock();
  unsigned int c = this->messages.size();
  this->mutex->unlock();
  return c;
}

//////////////////////////////////////////////////
/// Get the topic name
std::string Publisher::GetTopic() const
{
  return this->topic;
}

//////////////////////////////////////////////////
/// Get the message type
std::string Publisher::GetMsgType() const
{
  return this->msgType;
}

//////////////////////////////////////////////////
// Callback when a publish is completed
void Publisher::OnPublishComplete()
{
}

void Publisher::SetPublication(PublicationPtr &_publication, int _i)
{
  this->publications[_i] = _publication;
}

bool Publisher::GetLatching() const
{
  return this->latch;
}

std::string Publisher::GetPrevMsg() const
{
  return this->prevMsg;
}


