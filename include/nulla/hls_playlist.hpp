#ifndef __NULLA_HLS_PLAYLIST_HPP
#define __NULLA_HLS_PLAYLIST_HPP

#include "nulla/playlist.hpp"

#include <sstream>

namespace ioremap { namespace nulla {

class m3u8 {
public:
	m3u8(const nulla::playlist_t &playlist) : m_playlist(playlist) {
	}

	void generate() {
		m_ss << "#EXTM3U\n";
		m_ss << "#EXT-X-VERSION:3\n";

		const nulla::period &pr = m_playlist->periods.front();
		m_total_duration_msec = pr.duration_msec;

		// do not support multiple periods, use multiple tracks in the representation instead
		add_period(pr);
	}

	std::string main_playlist() const {
		return m_ss.str();
	}

	std::string variant_playlist(const std::string &prefix) const {
		auto it = m_variants.find(prefix);
		if (it == m_variants.end())
			return "";
		return it->second;
	}

private:
	nulla::playlist_t m_playlist;
	long m_total_duration_msec = 0;
	std::ostringstream m_ss;
	std::map<std::string, std::string> m_variants;

	int m_adaptation_id = 1;
	std::vector<std::string> m_audio_groups, m_video_groups;

	void add_period(const nulla::period &p) {
		BOOST_FOREACH(const nulla::adaptation &aset, p.adaptations) {
			std::string adaptation_id = "adaptation-" + std::to_string(m_adaptation_id);
			add_aset_groups(adaptation_id, aset);
			m_adaptation_id++;
		}

		BOOST_FOREACH(const nulla::adaptation &aset, p.adaptations) {
			add_aset(aset);
		}
	}

	void add_aset_groups(const std::string &adaptation_id, const nulla::adaptation &a) {
		std::string group_id;
		std::string type;

		BOOST_FOREACH(const std::string &id, a.repr_ids) {
			auto it = m_playlist->repr.find(id);
			if (it == m_playlist->repr.end())
				continue;

			const nulla::representation &r = it->second;

			const nulla::track_request &trf = r.tracks.front();
			const nulla::track &track = trf.track();

			std::string type = "DATA";
			if (track.media_type == GF_ISOM_MEDIA_AUDIO) {
				type = "AUDIO";
				group_id = "audio-" + std::to_string(m_audio_groups.size());
				m_audio_groups.push_back(group_id);
			}
			if (track.media_type == GF_ISOM_MEDIA_VISUAL) {
				type = "VIDEO";
				group_id = "video-" + std::to_string(m_video_groups.size());
				m_video_groups.push_back(group_id);
			}

			std::string url = m_playlist->base_url + "playlist/" + r.id;

			m_ss << "#EXT-X-MEDIA" <<
				":TYPE=" << type <<
				",GROUP-ID=\"" << group_id << "\"" <<
				",NAME=\"" << adaptation_id << "\"" <<
				//",LANGUAGE=\"" << a.lang << "\"" <<
				",AUTOSELECT=YES" <<
				",URI=\"" << url << "\"" <<
				"\n";
		}
	}

	void add_aset(const nulla::adaptation &a) {
		if (m_audio_groups.empty()) {
			m_audio_groups.push_back("none");
		}
		if (m_video_groups.empty()) {
			m_video_groups.push_back("none");
		}
		BOOST_FOREACH(const std::string &id, a.repr_ids) {
			auto it = m_playlist->repr.find(id);
			if (it == m_playlist->repr.end())
				continue;

			const nulla::representation &r = it->second;

			BOOST_FOREACH(std::string &atype, m_audio_groups) {
				BOOST_FOREACH(std::string &vtype, m_video_groups) {
					add_representation(m_ss, r, atype, vtype);
				}
			}
		}
	}

	void add_representation(std::ostringstream &ss, const nulla::representation &r,
			const std::string &atype, const std::string &vtype) {
		const nulla::track_request &trf = r.tracks.front();
		const nulla::track &track = trf.track();

		std::string url = m_playlist->base_url + "playlist/" + r.id;

		std::string codec = track.codec;
		if (codec.substr(0, 4) == "avc3") {
			codec[3] = '1';
		}

		ss << "#EXT-X-STREAM-INF"
			":PROGRAM-ID=1" << 
			",BANDWIDTH=" << track.bandwidth <<
			",CODECS=\"" << codec << "\""
		;

		if (track.media_type == GF_ISOM_MEDIA_VISUAL) {
			ss << ",RESOLUTION=" << track.video.width << "x" << track.video.height;
		}

		if (atype != "none") {
			ss << ",AUDIO=\"" << atype << "\"";
		}
		if (vtype != "none") {
			ss << ",VIDEO=\"" << vtype << "\"";
		}

		ss << "\n" << url << "\n";

		std::ostringstream pls;
		pls	<< "#EXTM3U\n"
			<< "#EXT-X-VERSION:3\n"
			<< "#EXT-X-PLAYLIST-TYPE:VOD\n"
			<< "#EXT-X-MEDIA-SEQUENCE:0\n"
			<< "#EXT-X-TARGETDURATION:" << m_playlist->chunk_duration_sec << "\n"
		;

		for (const nulla::track_request &tr: r.tracks) {
			float total_duration = 0;
			float track_duration = (float)tr.duration_msec / 1000.0;

			int track_in_msec = 1000 * m_playlist->chunk_duration_sec;
			int number_max = (tr.duration_msec + track_in_msec - 1) / track_in_msec;

			float duration = m_playlist->chunk_duration_sec;

			for (int i = 0; i < number_max; ++i) {
				if (i == number_max - 1)
					duration = track_duration - total_duration;

				pls	<< "#EXTINF:" << duration << ",\n"
					<< m_playlist->base_url + "play/" + r.id + "/" + std::to_string(tr.start_number + i) << "\n"
				;

				total_duration += duration;
			}
		}
		pls << "#EXT-X-ENDLIST";

		m_variants[r.id] = pls.str();
	}
};

}} // namespace ioremap::nulla

#endif // __NULLA_HLS_PLAYLIST_HPP
