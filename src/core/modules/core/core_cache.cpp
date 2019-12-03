/**
* =============================================================================
* Source Python
* Copyright (C) 2012-2019 Source Python Development Team.  All rights reserved.
* =============================================================================
*
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License, version 3.0, as published by the
* Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public License along with
* this program.  If not, see <http://www.gnu.org/licenses/>.
*
* As a special exception, the Source Python Team gives you permission
* to link the code of this program (as well as its derivative works) to
* "Half-Life 2," the "Source Engine," and any Game MODs that run on software
* by the Valve Corporation.  You must obey the GNU General Public License in
* all respects for all other code used.  Additionally, the Source.Python
* Development Team grants this exception to all derivative works.
*/

//-----------------------------------------------------------------------------
// Includes.
//-----------------------------------------------------------------------------
#include "core_cache.h"
#include "export_main.h"


//-----------------------------------------------------------------------------
// CCachedProperty class.
//-----------------------------------------------------------------------------
CCachedProperty::CCachedProperty(
	object fget=object(), object fset=object(), object fdel=object(), const char *doc=NULL,
	boost::python::tuple args=boost::python::tuple(), dict kwargs=dict())
{
	set_getter(fget);
	set_setter(fset);
	set_deleter(fdel);

	m_szDocString = doc;

	m_args = args;
	m_kwargs = kwargs;
}


object CCachedProperty::_callable_check(object function, const char *szName)
{
	if (!function.is_none() && !PyCallable_Check(function.ptr()))
		BOOST_RAISE_EXCEPTION(
			PyExc_TypeError,
			"The given %s function is not callable.", szName
		);

	return function;
}

object CCachedProperty::_prepare_value(object value)
{
	if (!PyGen_Check(value.ptr()))
		return value;

	if (getattr(value, "gi_frame").is_none())
		BOOST_RAISE_EXCEPTION(
			PyExc_ValueError,
			"The given generator is exhausted."
		);

	list values;
	while (true)
	{
		try
		{
			values.append(value.attr("__next__")());
		}
		catch(...)
		{
			PyErr_Clear();
			break;
		}
	}

	return values;
}


object CCachedProperty::get_getter()
{
	return m_fget;
}

object CCachedProperty::set_getter(object fget)
{
	m_fget = _callable_check(fget, "getter");
	return fget;
}


object CCachedProperty::get_setter()
{
	return m_fset;
}

object CCachedProperty::set_setter(object fset)
{
	m_fset = _callable_check(fset, "setter");
	return fset;
}


object CCachedProperty::get_deleter()
{
	return m_fdel;
}

object CCachedProperty::set_deleter(object fdel)
{
	m_fdel = _callable_check(fdel, "deleter");
	return fdel;
}


object CCachedProperty::get_owner()
{
	return m_owner;
}

str CCachedProperty::get_name()
{
	return m_name;
}


void CCachedProperty::__set_name__(object owner, str name)
{
	m_owner = owner;
	m_name = name;
}

object CCachedProperty::__get__(object instance, object owner=object())
{
	if (instance.is_none())
		return object(ptr(this));

	if (m_name.is_none())
		BOOST_RAISE_EXCEPTION(
			PyExc_AttributeError,
			"Unable to retrieve the value of an unbound property."
		);

	object cache = extract<dict>(instance.attr("__dict__"));

	try
	{
		return cache[m_name];
	}
	catch (...)
	{
		if (!PyErr_ExceptionMatches(PyExc_KeyError))
			throw_error_already_set();

		PyErr_Clear();

		if (m_fget.is_none())
			BOOST_RAISE_EXCEPTION(
				PyExc_AttributeError,
				"Unable to retrieve the value of a property that have no getter function."
			);

		cache[m_name] = _prepare_value(
			m_fget(
				*(make_tuple(handle<>(borrowed(instance.ptr()))) + m_args),
				**m_kwargs
			)
		);
	}

	return cache[m_name];
}


void CCachedProperty::__set__(object instance, object value)
{
	if (m_fset.is_none())
		BOOST_RAISE_EXCEPTION(
			PyExc_AttributeError,
			"Unable to assign the value of a property that have no setter function."
		);

	object result = m_fset(
		*(make_tuple(handle<>(borrowed(instance.ptr())), value) + m_args),
		**m_kwargs
	);

	dict cache = extract<dict>(instance.attr("__dict__"));
	if (!result.is_none())
		cache[m_name] = _prepare_value(result);
	else
		cache[m_name].del();
}

void CCachedProperty::__delete__(object instance)
{
	if (!m_fdel.is_none())
		m_fdel(
			*(make_tuple(handle<>(borrowed(instance.ptr()))) + m_args),
			**m_kwargs
		);

	dict cache = extract<dict>(instance.attr("__dict__"));
	cache[m_name].del();
}

object CCachedProperty::__call__(object fget)
{
	m_fget = _callable_check(fget, "getter");
	return object(ptr(this));
}

object CCachedProperty::__getitem__(str item)
{
	return m_kwargs[item];
}

void CCachedProperty::__setitem__(str item, object value)
{
	m_kwargs[item] = value;
}


CCachedProperty *CCachedProperty::wrap_descriptor(object descriptor, object owner=object(), str name=str())
{
	CCachedProperty *pProperty = new CCachedProperty(
		descriptor.attr("__get__"), descriptor.attr("__set__"), descriptor.attr("__delete__")
	);

	pProperty->m_szDocString = extract<const char *>(descriptor.attr("__doc__"));
	pProperty->__set_name__(owner, name);

	return pProperty;
}