#include "loop.hpp"
#include "make_unique.hpp"

#include <chrono>
#include <algorithm>
#include <array>
#include <thread>
#include <iostream>
#include <cstring>
#include <cassert>

#include <poll.h>

namespace saftbus {


	Source::Source() 
	{
		pfds_size = 0;
	}
	Source::~Source() = default;

	void Source::add_poll(struct pollfd &pfd)
	{
		assert(pfds_size < pfds.size());
		pfds[pfds_size++] = &pfd;
	}
	void Source::remove_poll(struct pollfd &pfd)
	{
		pfds_size = pfds.begin() - std::remove(pfds.begin(), pfds.end(), &pfd);
	}
	void Source::destroy() {
		loop->remove(this);
	}
	bool operator==(const std::unique_ptr<Source> &lhs, const Source *rhs)
	{
		return &*lhs == rhs;
	}

	//////////////////////////////
	//////////////////////////////
	//////////////////////////////

	struct Loop::Impl {
		std::vector<Source*> removed_sources;
		std::vector<std::unique_ptr<Source> > added_sources;
		std::vector<std::unique_ptr<Source> > sources;
		bool running;
		int running_depth; 
	};


	Loop::Loop() 
		: d(std2::make_unique<Impl>())
	{
		// reserve all the vectors with enough space to avoid 
		// dynamic allocation in normal operation
		const size_t revserve_that_much = 32;
		d->removed_sources.reserve(revserve_that_much);
		d->added_sources.reserve(revserve_that_much);
		d->sources.reserve(revserve_that_much);
		d->running = true;
		d->running_depth = 0; // 0 means: the loop is not running
	}
	Loop::~Loop() = default;

	Loop& Loop::get_default() {
		static Loop default_loop;
		return default_loop;
	}

	bool Loop::iteration(bool may_block) {
		unsigned us = 0;
		auto start = std::chrono::steady_clock::now();
		auto stop = std::chrono::steady_clock::now();
		++d->running_depth;
		// std::cerr << ".";
		static const auto no_timeout = std::chrono::milliseconds(-1);

		std::vector<struct pollfd> pfds;
		pfds.reserve(16);
		std::vector<struct pollfd*> source_pfds;
		source_pfds.reserve(16);


		//////////////////
		// preparation 
		// (find the earliest timeout)
		//////////////////
		auto timeout = no_timeout; 
		for(auto &source: d->sources) {
			auto timeout_source = no_timeout;
			source->prepare(timeout_source); // source may leave timeout_source unchanged 
			if (timeout_source != no_timeout) {
				if (timeout == no_timeout) {
					timeout = timeout_source;
				} else {
					timeout = std::min(timeout, timeout_source);
				}
			}
			for(auto it = source->pfds.cbegin(); it != source->pfds.cbegin()+source->pfds_size; ++it) {
				// create a packed array of pfds that can be passed to poll()
				pfds.push_back(**it);
				// also create an array of pointers to pfds to where the poll() results can be copied back
				source_pfds.push_back(*it);
			}
		}
		if (!may_block) {
			timeout = std::chrono::milliseconds(0);
		}
		//////////////////
		// polling / waiting
		//////////////////
		// std::cerr << "poll pfds size " << pfds.size() << std::endl;
		if (pfds.size() > 0) {
			// std::cerr << "p";
			int poll_result = 0;
			stop = std::chrono::steady_clock::now();
			us += std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();
			if ((poll_result = poll(&pfds[0], pfds.size(), timeout.count())) > 0) {
				for (unsigned i = 0; i < pfds.size();++i) {
					// copy the results back to the owners of the pfds
					// std::cerr << "poll results " << i << " = " << pfds[i].revents << std::endl;
					source_pfds[i]->revents = pfds[i].revents;
				}
			} else if (poll_result < 0) {
				std::cerr << "poll error: " << strerror(errno) << std::endl;
			} 
			start = std::chrono::steady_clock::now();

		} else if (timeout > std::chrono::milliseconds(0)) {
			stop = std::chrono::steady_clock::now();
			us += std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count();

			// std::cerr << "s";
			std::this_thread::sleep_for(timeout);
			start = std::chrono::steady_clock::now();
			
		}
		// start = std::chrono::steady_clock::now();

		//////////////////
		// dispatching
		//////////////////
		for (auto &source: d->sources) {
			if (source->check()) {
				// this is allowed(?) to make (nested) calls to Loop::iteration
				source->dispatch();
			}
		}

		//////////////////////////////////////////////////////
		// cleanup of finished sources
		// and addition of new sources
		// only if this is not a nested iteration
		//////////////////////////////////////////////////////
		bool changes = false;
		if (d->running_depth == 1) {
			// std::cerr << "cleaning up sources" << std::endl;
			for (auto removed_source: d->removed_sources) {
				// std::cerr << "cleaning a source sources" << std::endl;
				d->sources.erase(std::remove(d->sources.begin(), d->sources.end(), removed_source), 
					          d->sources.end());
				changes = true;
			}
			d->removed_sources.clear();

			// adding new sources
			for (auto &added_source: d->added_sources) {
				d->sources.push_back(std::move(added_source));
				changes = true;
			}
			d->added_sources.clear();
		}

		--d->running_depth;

		stop = std::chrono::steady_clock::now();
		std::cerr << changes << "    " << us << " , " << std::chrono::duration_cast<std::chrono::nanoseconds>(stop-start).count() << std::endl;

		return !d->sources.empty();
	}

	void Loop::run() {
		d->running = true;
		while (d->running) {
			if (!iteration(true)) {
				d->running = false;
			}
		}
	}

	bool Loop::quit() {
		return d->running = false;
	}

	bool Loop::quit_in(std::chrono::milliseconds wait_ms) {
		// //wait_ms = std::max(wait_ms, std::chrono::milliseconds(1)); // no less then 1 ms
		// connect(std::move(
		// 		std2::make_unique<saftbus::TimeoutSource>
		// 			(std::bind(&Loop::quit, this), wait_ms)
		// 	)
		// );
		connect<saftbus::TimeoutSource>(std::bind(&Loop::quit, this), wait_ms);
		return false;
	}

	void Loop::clear() {
		d->sources.clear();
	}

	bool Loop::connect(std::unique_ptr<Source> source) {
		source->loop = this;
		d->added_sources.push_back(std::move(source));
		return true;
	}

	void Loop::remove(Source *s) {
		d->removed_sources.push_back(s);
	}


	//////////////////////////////
	//////////////////////////////
	//////////////////////////////


	TimeoutSource::TimeoutSource(std::function<bool(void)> s, std::chrono::milliseconds i, std::chrono::milliseconds o) 
		: slot(s), interval(i), next_time(std::chrono::steady_clock::now()+i+o)
	{}

	TimeoutSource::~TimeoutSource() = default;

	bool TimeoutSource::prepare(std::chrono::milliseconds &timeout_ms) {
		auto now = std::chrono::steady_clock::now();
		if (now >= next_time) {
			timeout_ms = std::chrono::milliseconds(0);
			return true;
		}
		timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next_time - now); 
		return false;
	}

	bool TimeoutSource::check() {
		auto now = std::chrono::steady_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(next_time - now); 
		if (ms <= std::chrono::milliseconds(0)) {
			return true;
		}
		return false;
	}

	bool TimeoutSource::dispatch() {
		auto now = std::chrono::steady_clock::now();
		do {
			next_time += interval;
		} while (now >= next_time);
		auto result = slot();
		if (!result) {
			destroy();
		}
		return result;
	}



	//////////////////////////////
	//////////////////////////////
	//////////////////////////////


	IoSource::IoSource(std::function<bool(int, int)> s, int f, int c) 
		: slot(s)
	{
		pfd.fd            = f;
		pfd.events        = c;
		pfd.revents       = 0;
		// id = id_source++;
		// std::cerr << "IoSource(" << d->id << ")" << std::endl;
		// std::cerr << (condition & POLLHUP) << " " << (condition & POLLIN) << std::endl;
		add_poll(pfd);
	}
	IoSource::~IoSource() 
	{
		remove_poll(pfd);
	}

	bool IoSource::prepare(std::chrono::milliseconds &timeout_ms) {
		// std::cerr << "prepare IoSource(" << d->id << ")" << std::endl;
		if (pfd.revents & pfd.events) {
			return true;
		}
		return false;
	}

	bool IoSource::check() {
		// std::cerr << "check IoSource(" << d->id << ") " << d->pfd.revents << " "<< d->pfd.events << std::endl;
		if (pfd.revents & pfd.events) {
			return true;
		}
		return false;
	}

	bool IoSource::dispatch() {
		// std::cerr << "dispatch IoSource(" << d->id << ")" << std::endl;
		auto result = slot(pfd.fd, pfd.revents);
		if (!result) {
			// remove_poll(d->pfd);
			destroy();
		}
		pfd.revents = 0; // clear the events after  the dispatching
		return result;
	}




}