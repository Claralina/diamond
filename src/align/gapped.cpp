/****
DIAMOND protein aligner
Copyright (C) 2013-2020 Max Planck Society for the Advancement of Science e.V.
                        Benjamin Buchfink
                        Eberhard Karls Universitaet Tuebingen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#include <algorithm>
#include "target.h"
#include "../dp/dp.h"

using std::vector;
using std::array;
using std::list;

namespace Extension {

Match::Match(size_t target_block_id, bool outranked, std::array<std::list<Hsp>, MAX_CONTEXT> &hsps):
	target_block_id(target_block_id),
	outranked(outranked)
{
	for (unsigned i = 0; i < align_mode.query_contexts; ++i)
		hsp.splice(hsp.end(), hsps[i]);
	hsp.sort();
	if (!hsp.empty())
		filter_score = hsp.front().score;
	if (config.max_hsps > 0)
		max_hsp_culling();
}

void add_dp_targets(const WorkTarget &target, int target_idx, const sequence *query_seq, array<vector<DpTarget>, MAX_CONTEXT> &dp_targets) {
	const int band = config.padding > 0 ? config.padding : DEFAULT_BAND,
		slen = (int)target.seq.length();
	for (unsigned frame = 0; frame < align_mode.query_contexts; ++frame) {
		const int qlen = (int)query_seq[frame].length();
		for (const Hsp_traits &hsp : target.hsp[frame]) {
			if (config.log_extend) {
				cout << "i_begin=" << hsp.query_range.begin_ << " j_begin=" << hsp.subject_range.begin_ << " d_min=" << hsp.d_min << " d_max=" << hsp.d_max << endl;
			}
			dp_targets[frame].emplace_back(target.seq, std::max(hsp.d_min - band, -(slen - 1)), std::min(hsp.d_max + 1 + band, qlen), target_idx);
		}
	}
}

vector<Target> align(const vector<WorkTarget> &targets, const sequence *query_seq, const Bias_correction *query_cb, int flags, Statistics &stat) {
	const int raw_score_cutoff = score_matrix.rawscore(config.min_bit_score == 0 ? score_matrix.bitscore(config.max_evalue, (unsigned)query_seq[0].length()) : config.min_bit_score);

	array<vector<DpTarget>, MAX_CONTEXT> dp_targets;
	vector<Target> r;
	if (targets.empty())
		return r;
	r.reserve(targets.size());
	for (int i = 0; i < (int)targets.size(); ++i) {
		add_dp_targets(targets[i], i, query_seq, dp_targets);
		r.emplace_back(targets[i].block_id, targets[i].seq, targets[i].outranked);
	}

	for (unsigned frame = 0; frame < align_mode.query_contexts; ++frame) {
		if (dp_targets[frame].empty())
			continue;
		list<Hsp> hsp = DP::BandedSwipe::swipe(
			query_seq[frame],
			dp_targets[frame].begin(),
			dp_targets[frame].end(),
			Frame(frame),
			config.comp_based_stats ? &query_cb[frame] : nullptr,
			flags,
			raw_score_cutoff,
			stat);
		while (!hsp.empty())
			r[hsp.front().swipe_target].add_hit(hsp, hsp.begin());
	}

	return r;
}

void add_dp_targets(const Target &target, int target_idx, const sequence *query_seq, array<vector<DpTarget>, MAX_CONTEXT> &dp_targets) {
	const int band = config.padding > 0 ? config.padding : DEFAULT_BAND,
		slen = (int)target.seq.length();
	for (unsigned frame = 0; frame < align_mode.query_contexts; ++frame) {
		const int qlen = (int)query_seq[frame].length();
		for (const Hsp &hsp : target.hsp[frame]) {
			dp_targets[frame].emplace_back(target.seq, hsp.query_range.begin_, hsp.query_range.end_, target_idx);
		}
	}
}

vector<Match> align(vector<Target> &targets, const sequence *query_seq, const Bias_correction *query_cb, int source_query_len, int flags, Statistics &stat) {
	const int raw_score_cutoff = score_matrix.rawscore(config.min_bit_score == 0 ? score_matrix.bitscore(config.max_evalue, (unsigned)query_seq[0].length()) : config.min_bit_score);

	array<vector<DpTarget>, MAX_CONTEXT> dp_targets;
	vector<Match> r;
	if (targets.empty())
		return r;
	r.reserve(targets.size());

	if (config.disable_traceback) {
		for (Target &t : targets)
			r.emplace_back(t.block_id, t.outranked, t.hsp);
		return r;
	}

	for (int i = 0; i < (int)targets.size(); ++i) {
		add_dp_targets(targets[i], i, query_seq, dp_targets);
		r.emplace_back(targets[i].block_id, targets[i].outranked);
	}

	for (unsigned frame = 0; frame < align_mode.query_contexts; ++frame) {
		if (dp_targets[frame].empty())
			continue;
		list<Hsp> hsp = DP::BandedSwipe::swipe(
			query_seq[frame],
			dp_targets[frame].begin(),
			dp_targets[frame].end(),
			Frame(frame),
			config.comp_based_stats ? &query_cb[frame] : nullptr,
			DP::TRACEBACK | flags,
			raw_score_cutoff,
			stat);
		while (!hsp.empty())
			r[hsp.front().swipe_target].add_hit(hsp, hsp.begin());
	}

	for (Match &match : r)
		match.inner_culling(source_query_len);

	return r;
}

}
