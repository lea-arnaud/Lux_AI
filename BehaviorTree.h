#ifndef DEHAVIORTREE_H
#define BEHAVIORTREE_H

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <any>
#include <stdexcept>

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

	void insertData(const std::string& key, const std::any& value) {
		data[key] = value;
	}

	void removeData(const std::string &key) {
		data.erase(key);
	}

	template <typename T>
	T& getData(const std::string& key) {
		if (data.find(key) != data.end()) {
			try {
				return std::any_cast<T&>(data[key]);
			}
			catch (const std::bad_any_cast&) {
				throw std::runtime_error("Type mismatch when getting data from Blackboard for: " + key);
			}
		}
		else if (parent != nullptr) {
			return parent->getData<T>(key);
		}
		else {
			throw std::out_of_range("Key not found in Blackboard: " + key);
		}
	}
};

class Task {
public:
	// Return on success (true) or failure (false)
	virtual bool run(std::shared_ptr<Blackboard> blackboard) = 0;
};

class Selector : public Task {
	std::vector<std::shared_ptr<Task>> children;

public:
	bool run(std::shared_ptr<Blackboard> blackboard) override {
		for (const auto& task : children) {
			if (task->run(blackboard)) return true;
		}

		return false;
	}

	void addTask(std::shared_ptr<Task> task) {
		children.push_back(std::move(task));
	}
};

class Sequence : public Task {
	std::vector<std::shared_ptr<Task>> children;

public:
	bool run(std::shared_ptr<Blackboard> blackboard) override {
		for (const auto& task : children) {
			if (!task->run(blackboard)) return false;
		}

		return true;
	}

	void addTask(std::shared_ptr<Task> task) {
		children.push_back(std::move(task));
	}
};

class Decorator : public Task {
protected:
	std::shared_ptr<Task> child;
};

class BlackboardManager : public Decorator {
	std::shared_ptr<Blackboard> newBlackboard;

	bool run(std::shared_ptr<Blackboard> blackboard) override {
		newBlackboard = {blackboard};
		bool result = child->run(newBlackboard);
		return result;
	}
};

class BasicBehavior {
	std::shared_ptr<Task> rootTask;

public:
	BasicBehavior(std::shared_ptr<Task> root) : rootTask(root) {}

	bool run(std::shared_ptr<Blackboard> blackboard) {
		return rootTask->run(blackboard);
	}
};

#endif