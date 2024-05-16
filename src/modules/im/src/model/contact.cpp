/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */


#include "contact.h"
#include <QVariant>

Contact:: Contact(){

}

Contact:: Contact(const ContactId& id_, const QString& name_, const QString& alias_, bool isGroup_)
    : id(id_),name{name_},alias{alias_},group(isGroup_){

}

Contact::~Contact()
{

}

QString Contact::getDisplayedName() const {
  if (!alias.isEmpty()) {
    return alias;
  }
  return name;
}


void Contact::setName(const QString &_name) {

  if (_name.isEmpty()) {
    return;
  }


  if (_name == name) {
    return;
  }
    name = _name;
    emit nameChanged(name);

    if(alias.isEmpty()){
        emit displayedNameChanged(name);
    }

}


void Contact::setAlias(const QString &alias_) {
  if (alias_.isEmpty() || alias_ == alias) {
    return;
  }

  // save old displayed name to be able to compare for changes
  const auto oldDisplayed = getDisplayedName();
  alias = alias_;
  emit aliasChanged( alias);

  const auto newDisplayed = getDisplayedName();
  if (oldDisplayed != newDisplayed) {
    emit displayedNameChanged(newDisplayed);
  }
}
void Contact::setEventFlag(bool flag)
{

}

bool Contact::getEventFlag() const
{

    return false;
}
