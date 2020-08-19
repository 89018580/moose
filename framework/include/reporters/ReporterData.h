//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "RestartableData.h"
#include "ReporterState.h"
#include "ReporterContext.h"
#include "libmesh/parallel_object.h"
#include "libmesh/auto_ptr.h"
#include "MooseApp.h"

/**
 * This is a helper class for managing the storage of declared Reporter object values. This design
 * of the system is a generalization of the VectorPostprocessor system that the Reporter objects
 * replaced.
 *
 * Foremost, this object doesn't store the data. It simply acts as helper for using the restart
 * system of the MooseApp. This object automatically handles the old, older, ... data. All
 * declarations create std::pair<T, std::vector<T>> restartable data on the MooseApp, where the
 * first value is the current value and the data in the vector are the older data.
 *
 * The ReporterState object is a RestartableData object that serves as a helper for managing the
 * time history. A "context" object also exists that uses the ReporterState for preforming special
 * operations. Refer to ReporterState.h/C for more information.
 *
 * It is important to note that the Reporter values are not threaded. However, the Reporter
 * objects are UserObject based, so the calculation of the values can be threaded.
 *
 * This object also relies on ReporterName objects, which are simply a combination of the
 * Reporter object name and the data name. If you recall the VectorPostprocessor system on which
 * this is based required an object name and a vector name. The ReporterName class simply provides
 * a convenient way to provide that information in a single object. Special Parser syntax
 * was also defined so that application developers do not have to have input parameters for
 * both the object and data names (see Parser.C/h).
 */
class ReporterData
{
public:
  // The old/older values are stored in vector and this vector must have memory that doesn't
  // get reallocated. This is because the calls to getReporterValue can occur in any order using
  // any time index.
  constexpr static std::size_t HISTORY_CAPACITY = 2; // old and older

  ReporterData(MooseApp & moose_app);

  /**
   * Return True if a Reporter value with the given type and name have been created.
   */
  template <typename T>
  bool hasReporterValue(const ReporterName & reporter_name) const;

  /**
   * Return True if a Reporter value with any type existss with the given name.
   */
  bool hasReporterValue(const ReporterName & reporter_name) const;

  /**
   * Return a list of all reporter names
   */
  std::set<ReporterName> getReporterNames() const;

  /**
   * Method for returning read only references to Reporter values.
   * @tparam T The Reporter value C++ type.
   * @param reporter_name The name of the reporter value, which includes the object name and the
   *                      data name.
   * @param object_name The name of the object consuming the Reporter value (for error reporting)
   * @param mode The mode that the value will be consumed by the by the ReporterInterface object
   * @param time_index (optional) When not provided or zero is provided the current value is
   *                   returned. If an index greater than zero is provided then the corresponding
   *                   old data is returned (1 = old, 2 = older, etc.).
   */
  template <typename T>
  const T & getReporterValue(const ReporterName & reporter_name,
                             const std::string & object_name,
                             Moose::ReporterMode mode,
                             const std::size_t time_index = 0);

  /**
   * Method for returning a read-only reference to Reporter values that already exist.
   * @tparam T The Reporter value C++ type.
   * @param reporter_name The name of the reporter value, which includes the object name and the
   *                      data name.
   * @param time_index (optional) When not provided or zero is provided the current value is
   *                   returned. If an index greater than zero is provided then the corresponding
   *                   old data is returned (1 = old, 2 = older, etc.).
   */
  template <typename T>
  const T & getReporterValue(const ReporterName & reporter_name,
                             const std::size_t time_index = 0) const;

  /**
   * Method for setting Reporter values that already exist.
   * @tparam T The Reporter value C++ type.
   * @param reporter_name The name of the reporter value, which includes the object name and the
   *                      data name.
   * @param value The value to which the Reporter will be changed to.
   * @param time_index (optional) When not provided or zero is provided the current value is
   *                   returned. If an index greater than zero is provided then the corresponding
   *                   old data is returned (1 = old, 2 = older, etc.).
   * WARNING!
   * This method is designed for setting values outside of the traditional interfaces such as is
   * necessary for Transfers. This is an advanced capability that should be used with caution.
   *
   * @see FEProblemBase::setPostprocessorValueByName
   */
  template <typename T>
  void setReporterValue(const ReporterName & reporter_name,
                        const T & value,
                        const std::size_t time_index = 0);

  ///@{
  /**
   * Method for returning a writable reference to the current Reporter value. This method is
   * used by the Reporter class to produce values.
   * @tparam T The Reporter value C++ type.
   * @tparam S (optional) The ReporterContext for performing specialized actions after the values
   *           have been computed. For example, ReporterBroadcastContext automatically broadcasts
   *           the computed value. See ReporterState.C/h for more information.
   * @param reporter_name The name of the reporter value, which includes the object name and the
   *                      data name.
   * @param mode The mode that the produced value will be computed by the Reporter object
   * @param args (optional) Any number of optional arguments passed into the Context type given
   *             by the S template parameter. If S = ReporterContext then the first argument
   *             can be used as the default value (see ReporterContext.h).
   *
   * The ReporterContext objects allow for custom handling of data (e.g., broadcasting the value).
   * The get/declare methods can be called in any order thus an the underlying RestartableData
   * object is often created by the get method before it is declared. Therefore the custom
   * functionality cannot be handled by specializing the RestartableData/ReporterState object
   * directly because the state is often created prior to the declaration that dictates how the
   * produced value shall be computed. Thus, the reason for the separate ReporterContext objects.
   */
  template <typename T, template <typename> class S, typename... Args>
  T & declareReporterValue(const ReporterName & reporter_name,
                           Moose::ReporterMode mode,
                           Args &&... args);

  /**
   * Helper function for performing post calculation actions via the ReporterContext objects.
   *
   * If you recall, the original VectorPostprocessor system included the ability to perform some
   * scatter and broadcast actions via the special call on the storage helper object. This
   * is a replacement for that method that leverages the RepoorterContext objects to perform
   * value specific actions, including some automatic operations depending how the data is
   * produced and consumed.
   *
   * See FEProblemBase::joinAndFinalize
   */
  void finalize(const std::string & object_name);

  /**
   * At the end of a timestep this method is called to copy the values back in time in preparation
   * for the next timestep.
   *
   * See FEProblemBase::advanceState
   */
  void copyValuesBack();

  /**
   * Copies the current value to all the desired old values and shrinks the old value vector to
   * the correct size based on the requested values. This also analyzes the mode that the value
   * is produced verse the modes it is consumed to determine if communication is necessary or
   * that the data not compatable.
   *
   * See InitReporterAction
   */
  void init();

  /**
   * Performs error checking.
   *
   * @see FEProblemBase::checkIntegrity FEProblemBase::check Reporters
   */
  void check() const;

  /**
   * Return a set of undeclared names
   *
   * @see FEProblemBase::checkPostprocessors
   */
  std::set<ReporterName> getUndeclaredNames() const;

  /**
   * Writes all Reporter values to the supplied JSON node for output to a file.
   *
   * @see JSONOutput
   */
  void store(nlohmann::json & json) const;

private:
  /// For accessing the restart/recover system, which is where Reporter values are stored
  MooseApp & _app;

  /**
   * Helper object that for creating the necessary RestartableData for Reporter values.
   * @tparam T The desired C++ type for the Reporter value
   * @param reporter_name Object/data name for the Reporter value
   * @param declare Flag indicating if the ReporterValue is being declared or read. This flag
   *                is passed to the existing MooseApp restart/recover system that errors if a
   *                value is declared multiple times.
   */
  template <typename T>
  ReporterState<T> & getReporterStateHelper(const ReporterName & reporter_name, bool declare);

  /// The ReporterContext objects are created when a value is declared. The context objects
  /// include a reference to the associated ReporterState values. This container stores the
  /// context object for each Reporter value.
  std::set<std::unique_ptr<ReporterContextBase>> _context_ptrs;

  /// When true an error message is triggered so that the get/declare methods cannot be called
  /// outside of the object constructors.
  bool _initialized = false;

  /// Names of objects that have been retrieved but not declared
  std::set<ReporterName> _declare_names;
  std::set<ReporterName> _get_names;
};

template <typename T>
ReporterState<T> &
ReporterData::getReporterStateHelper(const ReporterName & reporter_name, bool declare)
{
  // Avoid calling get/declare methods after object construction
  if (_initialized)
    mooseError("An attempt was made to declare or get Reporter data with the name '",
               reporter_name,
               "' after Reporter data was initialized, calls to get or declare Reporter data "
               "should be made in the object constructor.");

  // Creates the RestartableData object for storage in the MooseApp restart/recover system
  auto data_ptr = libmesh_make_unique<ReporterState<T>>(reporter_name);

  // Limits the number of old/older values. It is possible to call get in any order so this
  // needs to be setup first
  data_ptr->get().second.resize(ReporterData::HISTORY_CAPACITY);
  RestartableDataValue & value =
      _app.registerRestartableData(data_ptr->name(), std::move(data_ptr), 0, !declare);
  auto & state_ref = static_cast<ReporterState<T> &>(value);
  return state_ref;
}

template <typename T>
const T &
ReporterData::getReporterValue(const ReporterName & reporter_name,
                               const std::string & object_name,
                               Moose::ReporterMode mode,
                               const std::size_t time_index)
{
  // Update get names list
  _get_names.insert(reporter_name);

  ReporterState<T> & state_ref = getReporterStateHelper<T>(reporter_name, false);
  state_ref.addConsumerMode(mode, object_name);
  return state_ref.value(time_index);
}

template <typename T, template <typename> class S, typename... Args>
T &
ReporterData::declareReporterValue(const ReporterName & reporter_name,
                                   Moose::ReporterMode mode,
                                   Args &&... args)
{
  // Update declared names list
  _declare_names.insert(reporter_name);

  // The mode for the ReporterState must be must be set before ReporterContext is created to allow
  // for custom context options to override the setting (e.g., BroadcastReporterContext)
  ReporterState<T> & state_ref = getReporterStateHelper<T>(reporter_name, true);
  state_ref.setProducerMode(mode);

  // Create the ReporterContext
  auto context_ptr = libmesh_make_unique<S<T>>(_app, state_ref, args...);
  _context_ptrs.emplace(std::move(context_ptr));
  return state_ref.value();
}

template <typename T>
bool
ReporterData::hasReporterValue(const ReporterName & reporter_name) const
{
  auto func = [reporter_name](const std::unique_ptr<ReporterContextBase> & ptr) {
    return ptr->name() == reporter_name;
  };
  auto ptr = std::find_if(_context_ptrs.begin(), _context_ptrs.end(), func);
  if (ptr != _context_ptrs.end())
  {
    auto context = dynamic_cast<const ReporterContext<T> *>(ptr->get());
    return context != nullptr;
  }
  return false;
}

template <typename T>
const T &
ReporterData::getReporterValue(const ReporterName & reporter_name,
                               const std::size_t time_index) const
{
  auto func = [reporter_name](const std::unique_ptr<ReporterContextBase> & ptr) {
    return ptr->name() == reporter_name;
  };
  auto ptr = std::find_if(_context_ptrs.begin(), _context_ptrs.end(), func);
  if (ptr == _context_ptrs.end())
    mooseError("The desired Reporter value '", reporter_name, "' does not exist.");
  auto context_ptr = static_cast<const ReporterContext<T> *>(ptr->get());
  return context_ptr->state().value(time_index);
}

template <typename T>
void
ReporterData::setReporterValue(const ReporterName & reporter_name,
                               const T & value,
                               const std::size_t time_index)
{
  // https://stackoverflow.com/questions/123758/how-do-i-remove-code-duplication-between-similar-const-and-non-const-member-func
  const auto & me = *this;
  const_cast<T &>(me.getReporterValue<T>(reporter_name, time_index)) = value;
}
