#ifndef BEHAVIORTREE_H
#define BEHAVIORTREE_H

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <any>
#include <stdexcept>
#include <array>
#include <functional>

class Blackboard {
	std::shared_ptr<Blackboard> parent;
	std::map<std::string, std::any> data;

public:
	Blackboard() : parent(nullptr) {}
	Blackboard(std::shared_ptr<Blackboard> parent) : parent(parent) {}

	Blackboard(const Blackboard &) = delete;
	Blackboard &operator=(const Blackboard &) = delete;

	Blackboard(Blackboard &&moved) noexcept
	  : parent(std::exchange(moved.parent, nullptr))
	  , data(std::exchange(moved.data, {}))
	{
	}

	Blackboard &operator=(Blackboard &&moved)
	{
	  parent = std::exchange(moved.parent, nullptr);
	  data = std::exchange(moved.data, {});
	  return *this;
	}

	void setParentBoard(const std::shared_ptr<Blackboard> &parentBlackboard)
	{
	  parent = parentBlackboard;
	}

	void insertData(const std::string& key, const std::any& value) {
		data[key] = value;
	}

	void updateData(const std::string& key, const std::any& value) {
	  if(data.contains(key)) {
		data[key] = value;
	  } else if(parent != nullptr) {
		parent->updateData(key, value);
	  } else {
		throw std::runtime_error("Tried to update non-existing key: " + key);
	  }
	}

	void removeData(const std::string &key) {
		data.erase(key);
	}

	bool hasData(const std::string &key) const {
	  return data.contains(key);
	}

	template <typename T>
	T& getData(const std::string& key) {
		if (data.find(key) != data.end()) {
			try {
				return std::any_cast<T&>(data[key]);
			} catch (const std::bad_any_cast&) {
				throw std::runtime_error("Type mismatch when getting data from Blackboard for: " + key
				  + " expected " + typeid(T).name() + " got " + data[key].type().name());
			}
		} else if (parent != nullptr) {
			return parent->getData<T>(key);
		} else {
			throw std::runtime_error("Key not found in Blackboard: " + key);
		}
	}

};

enum class TaskResult
{
  SUCCESS,
  PENDING,
  FAILURE,
};

class Task {
public:
	// Return on success (true) or failure (false)
	virtual TaskResult run(const std::shared_ptr<Blackboard> &blackboard) = 0;
};

class Test : public Task {
	using function_type = std::function<bool(Blackboard &)>;
	function_type m_test;

public:
	Test(function_type &&test) : m_test(std::move(test)) {}

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override
	{
		return m_test(*blackboard) ? TaskResult::SUCCESS : TaskResult::FAILURE;
	}
};

class SimpleAction : public Task {
	using function_type = std::function<void(Blackboard &)>;
	function_type m_action;

public:
	SimpleAction(function_type &&action) : m_action(std::move(action)) {}

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override
	{
		m_action(*blackboard);
		return TaskResult::SUCCESS;
	}
};

// function to Task adapter
class ComplexAction : public Task {
  using function_type = std::function<TaskResult(Blackboard &)>;
  function_type m_action;

public:
  ComplexAction(function_type &&action) : m_action(std::move(action)) {}

  TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override
  {
	return m_action(*blackboard);
  }
};

class Selector : public Task {
  std::vector<std::shared_ptr<Task>> children;

public:
  Selector() = default;
  Selector(auto &&...children) : children{ children... } {}

  TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override {
	for (const auto& task : children) {
	  TaskResult result = task->run(blackboard);
	  if (result == TaskResult::SUCCESS || result == TaskResult::PENDING) return result;
	}

	return TaskResult::FAILURE;
  }

  void addTask(std::shared_ptr<Task> task) {
	children.push_back(std::move(task));
  }
};

class Alternative : public Task {
  std::shared_ptr<Task> m_condition, m_branchTrue, m_branchFalse;
public:
  Alternative(const std::shared_ptr<Task> &condition, const std::shared_ptr<Task> &branchTrue, const std::shared_ptr<Task> &branchFalse)
	: m_condition{ condition }, m_branchTrue(branchTrue), m_branchFalse(branchFalse) {}

  TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override {
	TaskResult result = m_condition->run(blackboard);
	return result == TaskResult::PENDING ? result
	  : result == TaskResult::SUCCESS ? m_branchTrue->run(blackboard)
	  : m_branchFalse->run(blackboard);
  }
};

class Sequence : public Task {
	std::vector<std::shared_ptr<Task>> children;

public:
	Sequence() = default;
	Sequence(auto &&...children) : children{ children... } {}

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override {
		for (const auto& task : children) {
			TaskResult result = task->run(blackboard);
			if (result == TaskResult::FAILURE || result == TaskResult::PENDING) return result;
		}

		return TaskResult::SUCCESS;
	}

	void addTask(std::shared_ptr<Task> task) {
		children.push_back(std::move(task));
	}
};

class Decorator : public Task {
protected:
	std::shared_ptr<Task> child;

	Decorator(std::shared_ptr<Task> child) : child(child) {}
};

class WithResult : public Decorator
{
	TaskResult result;
public:
    WithResult(TaskResult result, std::shared_ptr<Task> child) : Decorator(child), result(result) {}
    WithResult(TaskResult result) : Decorator(nullptr), result(result) {}

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override {
		if(child != nullptr) child->run(blackboard);
		return result;
	}
};

class BlackboardManager : public Decorator {
	std::shared_ptr<Blackboard> newBlackboard;

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) override {
		newBlackboard = { blackboard }; // FIX this is a pointer copy, was it supposed to be a copy of the blackboard ?
		                                // also this is not thread-safe, is there a reason why newBlackboard is an instance member ?
		return child->run(newBlackboard);
	}
};

class BasicBehavior {
	std::shared_ptr<Task> rootTask;

public:
	BasicBehavior(std::shared_ptr<Task> root) : rootTask(root) {}

	TaskResult run(const std::shared_ptr<Blackboard> &blackboard) {
		return rootTask->run(blackboard);
	}
};

#endif