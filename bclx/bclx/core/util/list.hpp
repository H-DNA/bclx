#pragma once

#include "header.h"

namespace sds
{

class list
{
public:
	list();
	list(const gptr<header>& ptr);
	~list();
	header pop_front();

private:
	gptr<header> front;
};
	
} /* namespace sds */

sds::list::list() : front{nullptr} {}

sds::list::list(const gptr<header>& ptr) : front{ptr} {}

sds::list::~list() {}

header sds::list::pop_front()
{
	if (front == nullptr)
		return nullptr;
	header res = BCL::load(front);	// local
	front = res.next;
	return res;
}

