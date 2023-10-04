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

	void insertData(const std::string& key, const std::any& value) {
		data[key] = value;
	}

	template <typename T>
	T& getData(const std::string& key) {
		if (data.find(key) != data.end()) {
			try {
				return std::any_cast<T&>(data[key]);
			}
			catch (const std::bad_any_cast&) {
				throw std::runtime_error("Type mismatch when getting data from Blackboard");
			}
		}
		else if (parent != nullptr) {
			return parent->getData<T>(key);
		}
		else {
			throw std::out_of_range("Key not found in Blackboard");
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

class FollowPath : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class IsPathFound : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class IsFinished : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class IsSearching : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class Explore : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class FindPath : public Task {
public:
	bool run(std::shared_ptr<Blackboard> blackboard);
};

class BasicBehavior {
	std::shared_ptr<Task> rootTask;

public:
	BasicBehavior(std::shared_ptr<Task> root) : rootTask(root) {}

	bool run(std::shared_ptr<Blackboard> blackboard) {
		return rootTask->run(blackboard);
	}
};

/*
Selector
|
|-- Sequence
|   |
|   |-- IsPathFound
|   |
|   |-- FollowPath
|
|-- Sequence
|   |
|   |-- IsSearching
|   |
|   |-- Selector
|   |   |
|   |   |-- Sequence
|   |   |   |
|   |   |   |-- FindPath
|   |   |   |
|   |   |   |-- FollowPath
|   |   |
|   |   |-- Sequence
|   |       |
|   |       |-- Explore
|   |       |
|   |       |-- FollowPath
|   
|
|-- IsFinished
*/

inline BasicBehavior createSimpleBehaviorTree() {

	// Créer les nœuds de tâches pour chaque séquence
	auto isPathFoundTask = std::make_shared<IsPathFound>();
	auto followPathTask = std::make_shared<FollowPath>();
	auto isSearchingTask = std::make_shared<IsSearching>();
	auto findPathTask = std::make_shared<FindPath>();
	auto exploreTask = std::make_shared<Explore>();
	auto isFinishedTask = std::make_shared<IsFinished>();

	auto sequence1 = std::make_shared<Sequence>();
	sequence1->addTask(findPathTask);
	sequence1->addTask(followPathTask);

	auto sequence2 = std::make_shared<Sequence>();
	sequence2->addTask(exploreTask);
	sequence2->addTask(followPathTask);

	auto selector1 = std::make_shared<Selector>();
	selector1->addTask(sequence1);
	selector1->addTask(sequence2);

	auto sequence3 = std::make_shared<Sequence>();
	sequence3->addTask(isPathFoundTask);
	sequence3->addTask(followPathTask);

	auto sequence4 = std::make_shared<Sequence>();
	sequence4->addTask(isSearchingTask);
	sequence4->addTask(selector1);

	auto selector2 = std::make_shared<Selector>();
	selector2->addTask(sequence3);
	selector2->addTask(sequence4);
	selector2->addTask(isFinishedTask);


	// Créer un objet SimpleBehavior avec le sélecteur racine
	BasicBehavior basicBehavior(selector2);

	return basicBehavior;
}

#endif