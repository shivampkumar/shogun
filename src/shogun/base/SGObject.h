/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2008-2010 Soeren Sonnenburg
 * Written (W) 2011-2013 Heiko Strathmann
 * Written (W) 2013-2014 Thoralf Klein
 * Copyright (C) 2008-2010 Fraunhofer Institute FIRST and Max Planck Society
 */

#ifndef __SGOBJECT_H__
#define __SGOBJECT_H__

#include <shogun/base/AnyParameter.h>
#include <shogun/base/Version.h>
#include <shogun/base/unique.h>
#include <shogun/io/SGIO.h>
#include <shogun/lib/DataType.h>
#include <shogun/lib/RxCppHeader.h>
#include <shogun/lib/ShogunException.h>
#include <shogun/lib/any.h>
#include <shogun/lib/common.h>
#include <shogun/lib/config.h>
#include <shogun/lib/parameter_observers/ObservedValue.h>
#include <shogun/lib/tag.h>

#include <utility>

/** \namespace shogun
 * @brief all of classes and functions are contained in the shogun namespace
 */
namespace shogun
{
class RefCount;
class SGIO;
class Parallel;
class Parameter;
class CSerializableFile;
class ParameterObserverInterface;

template <class T, class K> class CMap;

struct TParameter;
template <class T> class DynArray;
template <class T> class SGStringList;

/*******************************************************************************
 * define reference counter macros
 ******************************************************************************/

#define SG_REF(x) { if (x) (x)->ref(); }
#define SG_UNREF(x) { if (x) { if ((x)->unref()==0) (x)=NULL; } }
#define SG_UNREF_NO_NULL(x) { if (x) { (x)->unref(); } }

/*******************************************************************************
 * Macros for registering parameters/model selection parameters
 ******************************************************************************/

#ifdef _MSC_VER

#define VA_NARGS(...)  INTERNAL_EXPAND_ARGS_PRIVATE(INTERNAL_ARGS_AUGMENTER(__VA_ARGS__))
#define INTERNAL_ARGS_AUGMENTER(...) unused, __VA_ARGS__
#define INTERNAL_EXPAND(x) x
#define INTERNAL_EXPAND_ARGS_PRIVATE(...) INTERNAL_EXPAND(INTERNAL_GET_ARG_COUNT_PRIVATE(__VA_ARGS__, 5, 4, 3, 2, 1, 0))
#define INTERNAL_GET_ARG_COUNT_PRIVATE(_0_, _1_, _2_, _3_, _4_, _5_, count, ...) count

#else

#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)

#endif

#define VARARG_IMPL2(base, count, ...) base##count(__VA_ARGS__)
#define VARARG_IMPL(base, count, ...) VARARG_IMPL2(base, count, __VA_ARGS__)
#define VARARG(base, ...) VARARG_IMPL(base, VA_NARGS(__VA_ARGS__), __VA_ARGS__)

#define SG_ADD4(param, name, description, ms_available)                        \
	{                                                                          \
		m_parameters->add(param, name, description);                           \
		watch_param(                                                           \
		    name, param,                                                       \
		    AnyParameterProperties(ms_available, GRADIENT_NOT_AVAILABLE));     \
		if (ms_available)                                                      \
			m_model_selection_parameters->add(param, name, description);       \
	}

#define SG_ADD5(param, name, description, ms_available, gradient_available)    \
	{                                                                          \
		m_parameters->add(param, name, description);                           \
		watch_param(                                                           \
		    name, param,                                                       \
		    AnyParameterProperties(ms_available, gradient_available));         \
		if (ms_available)                                                      \
			m_model_selection_parameters->add(param, name, description);       \
		if (gradient_available)                                                \
			m_gradient_parameters->add(param, name, description);              \
	}

#define SG_ADD(...) VARARG(SG_ADD, __VA_ARGS__)

/*******************************************************************************
 * End of macros for registering parameters/model selection parameters
 ******************************************************************************/

/** @brief Class SGObject is the base class of all shogun objects.
 *
 * Apart from dealing with reference counting that is used to manage shogung
 * objects in memory (erase unused object, avoid cleaning objects when they are
 * still in use), it provides interfaces for:
 *
 * -# parallel - to determine the number of used CPUs for a method (cf. Parallel)
 * -# io - to output messages and general i/o (cf. IO)
 * -# version - to provide version information of the shogun version used (cf. Version)
 *
 * All objects can be cloned and compared (deep copy, recursively)
 */
class CSGObject
{
public:
	typedef rxcpp::subjects::subject<ObservedValue> SGSubject;
	typedef rxcpp::observable<ObservedValue,
		                      rxcpp::dynamic_observable<ObservedValue>>
		SGObservable;
	typedef rxcpp::subscriber<
		ObservedValue, rxcpp::observer<ObservedValue, void, void, void, void>>
		SGSubscriber;

	/** default constructor */
	CSGObject();

	/** copy constructor */
	CSGObject(const CSGObject& orig);

	/** destructor */
	virtual ~CSGObject();

	/** increase reference counter
	 *
	 * @return reference count
	 */
	int32_t ref();

	/** display reference counter
	 *
	 * @return reference count
	 */
	int32_t ref_count();

	/** decrement reference counter and deallocate object if refcount is zero
	 * before or after decrementing it
	 *
	 * @return reference count
	 */
	int32_t unref();

#ifdef TRACE_MEMORY_ALLOCS
	static void list_memory_allocs();
#endif

	/** A shallow copy.
	 * All the SGObject instance variables will be simply assigned and SG_REF-ed.
	 */
	virtual CSGObject *shallow_copy() const;

	/** A deep copy.
	 * All the instance variables will also be copied.
	 */
	virtual CSGObject *deep_copy() const;

	/** Returns the name of the SGSerializable instance.  It MUST BE
	 *  the CLASS NAME without the prefixed `C'.
	 *
	 *  @return name of the SGSerializable
	 */
	virtual const char* get_name() const = 0;

	/** If the SGSerializable is a class template then TRUE will be
	 *  returned and GENERIC is set to the type of the generic.
	 *
	 *  @param generic set to the type of the generic if returning
	 *                 TRUE
	 *
	 *  @return TRUE if a class template.
	 */
	virtual bool is_generic(EPrimitiveType* generic) const;

	/** set generic type to T
	 */
	template<class T> void set_generic();

	/** unset generic type
	 *
	 * this has to be called in classes specializing a template class
	 */
	void unset_generic();

	/** prints registered parameters out
	 *
	 *	@param prefix prefix for members
	 */
	virtual void print_serializable(const char* prefix="");

	/** Save this object to file.
	 *
	 * @param file where to save the object; will be closed during
	 * returning if PREFIX is an empty string.
	 * @param prefix prefix for members
	 * @return TRUE if done, otherwise FALSE
	 */
	virtual bool save_serializable(CSerializableFile* file,
			const char* prefix="");

	/** Load this object from file.  If it will fail (returning FALSE)
	 *  then this object will contain inconsistent data and should not
	 *  be used!
	 *
	 *  @param file where to load from
	 *  @param prefix prefix for members
	 *
	 *  @return TRUE if done, otherwise FALSE
	 */
	virtual bool load_serializable(CSerializableFile* file,
			const char* prefix="");

	/** set the io object
	 *
	 * @param io io object to use
	 */
	void set_global_io(SGIO* io);

	/** get the io object
	 *
	 * @return io object
	 */
	SGIO* get_global_io();

	/** set the parallel object
	 *
	 * @param parallel parallel object to use
	 */
	void set_global_parallel(Parallel* parallel);

	/** get the parallel object
	 *
	 * @return parallel object
	 */
	Parallel* get_global_parallel();

	/** set the version object
	 *
	 * @param version version object to use
	 */
	void set_global_version(Version* version);

	/** get the version object
	 *
	 * @return version object
	 */
	Version* get_global_version();

	/** @return vector of names of all parameters which are registered for model
	 * selection */
	SGStringList<char> get_modelsel_names();

	/** prints all parameter registered for model selection and their type */
	void print_modsel_params();

	/** Returns description of a given parameter string, if it exists. SG_ERROR
	 * otherwise
	 *
	 * @param param_name name of the parameter
	 * @return description of the parameter
	 */
	char* get_modsel_param_descr(const char* param_name);

	/** Returns index of model selection parameter with provided index
	 *
	 * @param param_name name of model selection parameter
	 * @return index of model selection parameter with provided name,
	 * -1 if there is no such
	 */
	index_t get_modsel_param_index(const char* param_name);

	/** Builds a dictionary of all parameters in SGObject as well of those
	 *  of SGObjects that are parameters of this object. Dictionary maps
	 *  parameters to the objects that own them.
	 *
	 * @param dict dictionary of parameters to be built.
	 */
	void build_gradient_parameter_dictionary(CMap<TParameter*, CSGObject*>* dict);

	/** Checks if object has a class parameter identified by a name.
	 *
	 * @param name name of the parameter
	 * @return true if the parameter exists with the input name
	 */
	bool has(const std::string& name) const
	{
		return has_parameter(BaseTag(name));
	}

	/** Checks if object has a class parameter identified by a Tag.
	 *
	 * @param tag tag of the parameter containing name and type information
	 * @return true if the parameter exists with the input tag
	 */
	template <typename T>
	bool has(const Tag<T>& tag) const
	{
		return has<T>(tag.name());
	}

	/** Checks if a type exists for a class parameter identified by a name.
	 *
	 * @param name name of the parameter
	 * @return true if the parameter exists with the input name and type
	 */
	template <typename T, typename U=void>
	bool has(const std::string& name) const
	{
		BaseTag tag(name);
		if (!has_parameter(tag))
			return false;
		const Any value = get_parameter(tag).get_value();
		return value.same_type<T>();
	}

	/** Setter for a class parameter, identified by a Tag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name and type information of parameter
	 * @param value value of the parameter
	 */
	template <typename T>
	void put(const Tag<T>& _tag, const T& value)
	{
		if (has_parameter(_tag))
		{
			if(has<T>(_tag.name()))
				put_parameter(_tag, AnyParameter(erase_type(value)));
			else
			{
				SG_ERROR("Type for parameter with name \"%s\" is not correct.\n",
					_tag.name().c_str());
			}
		}
		else
		{
			SG_ERROR("\"%s\" does not have a parameter with name \"%s\".\n",
				get_name(), _tag.name().c_str());
		}
	}

	/** Setter for a class parameter, identified by a name.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter along with type information
	 */
	template <typename T, typename U = void>
	void put(const std::string& name, const T& value)
	{
		Tag<T> tag(name);
		put(tag, value);
	}

	/** Getter for a class parameter, identified by a Tag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name and type information of parameter
	 * @return value of the parameter identified by the input tag
	 */
	template <typename T>
	T get(const Tag<T>& _tag) const
	{
		const Any value = get_parameter(_tag).get_value();
		try
		{
			return recall_type<T>(value);
		}
		catch (const std::logic_error&)
		{
			SG_ERROR("Type for parameter with name \"%s\" is not correct in \"%s\".\n",
					_tag.name().c_str(), get_name());
		}
		// we won't be there
		return recall_type<T>(value);
	}

	/** Getter for a class parameter, identified by a name.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param name name of the parameter
	 * @return value of the parameter corresponding to the input name and type
	 */
	template <typename T, typename U=void>
	T get(const std::string& name) const
	{
		Tag<T> tag(name);
		return get(tag);
	}

#ifndef SWIG
	/**
	  * Get parameters observable
	  * @return RxCpp observable
	  */
	SGObservable* get_parameters_observable()
	{
		return m_observable_params;
	};
#endif

	/** Subscribe a parameter observer to watch over params */
	void subscribe_to_parameters(ParameterObserverInterface* obs);

	/** Print to stdout a list of observable parameters */
	void list_observable_parameters();

protected:
	/** Can (optionally) be overridden to pre-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::LOAD_SERIALIZABLE_PRE
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void load_serializable_pre() throw (ShogunException);

	/** Can (optionally) be overridden to post-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::LOAD_SERIALIZABLE_POST
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void load_serializable_post() throw (ShogunException);

	/** Can (optionally) be overridden to pre-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::SAVE_SERIALIZABLE_PRE
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void save_serializable_pre() throw (ShogunException);

	/** Can (optionally) be overridden to post-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::SAVE_SERIALIZABLE_POST
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void save_serializable_post() throw (ShogunException);

	/** Registers a class parameter which is identified by a tag.
	 * This enables the parameter to be modified by put() and retrieved by
	 * get().
	 * Parameters can be registered in the constructor of the class.
	 *
	 * @param _tag name and type information of parameter
	 * @param value value of the parameter
	 */
	template <typename T>
	void register_param(Tag<T>& _tag, const T& value)
	{
		put_parameter(_tag, AnyParameter(erase_type(value)));
	}

	/** Registers a class parameter which is identified by a name.
	 * This enables the parameter to be modified by put() and retrieved by
	 * get().
	 * Parameters can be registered in the constructor of the class.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter along with type information
	 */
	template <typename T>
	void register_param(const std::string& name, const T& value)
	{
		BaseTag tag(name);
		put_parameter(tag, AnyParameter(erase_type(value)));
	}

	template <typename T>
	void watch_param(
		const std::string& name, T* value, AnyParameterProperties properties)
	{
		BaseTag tag(name);
		put_parameter(
			tag, AnyParameter(erase_type_non_owning(value), properties));
	}

public:
	/** Updates the hash of current parameter combination */
	virtual void update_parameter_hash();

	/**
	 * @return whether parameter combination has changed since last update
	 */
	virtual bool parameter_hash_changed();

	/** Recursively compares the current SGObject to another one. Compares all
	 * registered numerical parameters, recursion upon complex (SGObject)
	 * parameters. Does not compare pointers!
	 *
	 * May be overwritten but please do with care! Should not be necessary in
	 * most cases.
	 *
	 * @param other object to compare with
	 * @param accuracy accuracy to use for comparison (optional)
	 * @param tolerant allows linient check on float equality (within accuracy)
	 * @return true if all parameters were equal, false if not
	 */
	virtual bool equals(CSGObject* other, float64_t accuracy=0.0, bool tolerant=false);

	/** Creates a clone of the current object. This is done via recursively
	 * traversing all parameters, which corresponds to a deep copy.
	 * Calling equals on the cloned object always returns true although none
	 * of the memory of both objects overlaps.
	 *
	 * @return an identical copy of the given object, which is disjoint in memory.
	 * NULL if the clone fails. Note that the returned object is SG_REF'ed
	 */
	virtual CSGObject* clone();

protected:
	/* Iteratively clones all parameters of the provided instance into this instance.
	 * This will fail if the objects have different sets of registered parameters,
	 * or if they have a different type as defined by get_name().
	 *
	 * @param other object whose parameters are to be cloned into this instance
	 * @return true if cloning was successful
	 */
	bool clone_parameters(CSGObject* other);

private:
	void set_global_objects();
	void unset_global_objects();
	void init();

	/** Checks if object has a parameter identified by a BaseTag.
	 * This only checks for name and not type information.
	 * See its usage in has() and has<T>().
	 *
	 * @param _tag name information of parameter
	 * @return true if the parameter exists with the input tag
	 */
	bool has_parameter(const BaseTag& _tag) const;

	/** Registers and modifies a class parameter, identified by a BaseTag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name information of parameter
	 * @param any value without type information of the parameter
	 */
	void put_parameter(const BaseTag& _tag, const AnyParameter& any);

	/** Getter for a class parameter, identified by a BaseTag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name information of parameter
	 * @return value of the parameter identified by the input tag
	 */
	AnyParameter get_parameter(const BaseTag& _tag) const;

	/** Gets an incremental hash of all parameters as well as the parameters of
	 * CSGObject children of the current object's parameters.
	 *
	 * @param hash the computed hash returned by reference
	 * @param carry value for Murmur3 incremental hash
	 * @param total_length total byte length of all hashed parameters so
	 * far. Byte length of parameters will be added to the total length
	 */
	void get_parameter_incremental_hash(uint32_t& hash, uint32_t& carry,
			uint32_t& total_length);

	class Self;
	Unique<Self> self;

	class ParameterObserverList;
	Unique<ParameterObserverList> param_obs_list;

protected:
	/**
	 * Observe a parameter value and emit them to observer.
	 * @param value Observed parameter's value
	 */
	void observe(const ObservedValue value);

	/**
	 * Register which params this object can emit.
	 * @param name the param name
	 * @param type the param type
	 * @param description a user oriented description
	 */
	void register_observable_param(
		const std::string& name, const SG_OBS_VALUE_TYPE type,
		const std::string& description);

public:
	/** io */
	SGIO* io;

	/** parallel */
	Parallel* parallel;

	/** version */
	Version* version;

	/** parameters */
	Parameter* m_parameters;

	/** model selection parameters */
	Parameter* m_model_selection_parameters;

	/** parameters wrt which we can compute gradients */
	Parameter* m_gradient_parameters;

	/** Hash of parameter values*/
	uint32_t m_hash;

private:

	EPrimitiveType m_generic;
	bool m_load_pre_called;
	bool m_load_post_called;
	bool m_save_pre_called;
	bool m_save_post_called;

	RefCount* m_refcount;

	/** Subject used to create the params observer */
	SGSubject* m_subject_params;

	/** Parameter Observable */
	SGObservable* m_observable_params;

	/** Subscriber used to call onNext, onComplete etc.*/
	SGSubscriber* m_subscriber_params;
};
}
#endif // __SGOBJECT_H__
