#include "progress.h"
#include <indicators/indicators.hpp>
#include <iostream>
#include <memory>
#include <string>

struct progress {
	indicators::DynamicProgress<indicators::ProgressBar> bars;
};

struct prog_bar {
	indicators::ProgressBar *bar;
	progress *owner;
	std::string name;
};

extern "C" {

progress *progress_new(bool threaded)
{
	(void)threaded;
	return new progress;
}

void progress_free(progress *p)
{
	if (!p)
		return;
	delete p;
}

void progress_bar_free(prog_bar *b)
{
	if (!b)
		return;
	delete b->bar;
	delete b;
}

prog_bar *progress_bar_add(progress *p, const char *name, size_t total)
{
	auto *b = new prog_bar;
	b->owner = p;
	b->name = name;
	auto init_prefix = std::string(name) + ": ";
	if (init_prefix.length() < 50)
		init_prefix.append(50 - init_prefix.length(), ' ');
	b->bar = new indicators::ProgressBar(
		indicators::option::BarWidth{30}, indicators::option::Stream{std::cerr}, indicators::option::Start{"["},
		indicators::option::End{"]"}, indicators::option::PrefixText{init_prefix},
		indicators::option::ShowElapsedTime{false}, indicators::option::ShowRemainingTime{false},
		indicators::option::MaxProgress{100},
		indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}});
	p->bars.push_back(*b->bar);
	p->bars.set_stream(std::cerr);
	return b;
}

void progress_bar_update(prog_bar *b, size_t done, size_t total, const char *desc)
{
	if (!b)
		return;

	b->bar->set_option(indicators::option::Completed{false});
	long pct = total > 0 ? (long)(done * 100 / total) : 0;
	b->bar->set_progress(pct);

	std::string prefix = b->name + ": ";
	if (desc && desc[0] != '\0')
		prefix += std::string(desc) + " ";
	if (prefix.length() < 50)
		prefix.append(50 - prefix.length(), ' ');
	else
		prefix = prefix.substr(0, 47) + "...";
	b->bar->set_option(indicators::option::PrefixText{prefix});

	std::string postfix = std::to_string(done) + "/" + std::to_string(total);
	b->bar->set_option(indicators::option::PostfixText{postfix});

	b->owner->bars.print_progress();
}

void progress_bar_tick(prog_bar *b, size_t total, const char *desc)
{
	if (!b)
		return;
	b->bar->tick();

	std::string prefix = b->name + ": ";
	if (desc && desc[0] != '\0')
		prefix += std::string(desc) + " ";
	if (prefix.length() < 50)
		prefix.append(50 - prefix.length(), ' ');
	else
		prefix = prefix.substr(0, 47) + "...";
	b->bar->set_option(indicators::option::PrefixText{prefix});

	b->owner->bars.print_progress();
}

void progress_entry_update(void *entry, size_t cur, size_t total, const char *desc)
{
	progress_bar_update(static_cast<prog_bar *>(entry), cur, total, desc);
}

void progress_entry_clear(void *entry)
{
	/* auto *b = static_cast<prog_bar *>(entry);
	 * if (!b)
	 *     return;
	 * b->bar->mark_as_completed();
	 * b->owner->bars.print_progress(); */
	(void)entry;
}

} // extern "C"
