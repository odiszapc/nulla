#ifndef __NULLA_MPD_GENERATOR_HPP
#define __NULLA_MPD_GENERATOR_HPP

#include "nulla/playlist.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>
#include <string>

namespace ioremap { namespace nulla {

namespace {
	namespace pt = boost::property_tree;
}

class mpd {
public:
	mpd(const nulla::playlist_t &playlist) : m_playlist(playlist) {
	}

	void generate() {
		pt::ptree mpd;
		mpd.put("<xmlattr>.xmlns", "urn:mpeg:dash:schema:mpd:2011");
		mpd.put("<xmlattr>.minBufferTime", "PT1.500S");
		mpd.put("<xmlattr>.profiles", "urn:mpeg:dash:profile:full:2011");
		mpd.put("<xmlattr>.type", "static");

		mpd.put("BaseURL", m_playlist->base_url);

		pt::ptree period;
		//period.put("<xmlattr>.duration", print_time(m_playlist->duration_msec));
		period.put("<xmlattr>.id", "period_id");

		for (const auto &repr_pair: m_playlist->repr) {
			const nulla::representation &repr = repr_pair.second;

			if (repr.tracks.empty())
				continue;

			pt::ptree aset;
			aset.put("<xmlattr>.segmentAlignment", "true");

			add_representation(aset, repr);
			period.add_child("AdaptationSet", aset);
		}
		mpd.add_child("Period", period);

		mpd.put("<xmlattr>.mediaPresentationDuration", print_time(m_playlist->duration_msec));
		mpd.put("<xmlattr>.maxSegmentDuration", print_time(m_playlist->duration_msec));
		m_root.add_child("MPD", mpd);
	}

	std::string xml() const {
		std::ostringstream ss;
		pt::write_xml(ss, m_root);
		return ss.str();
	}


private:
	nulla::playlist_t m_playlist;
	pt::ptree m_root;

	void add_representation(pt::ptree &aset, const nulla::representation &r) {
		pt::ptree repr;

		repr.put("<xmlattr>.id", r.id);
		repr.put("<xmlattr>.startWithSAP", "1");

		// we generate MPD based on the very first track request
		// it is not allowed to change codec or bandwidth for example in the subsequent
		// track requests
		//
		// each track request can have multiple tracks, we are only interested in the @requested_track_number
		// this number can differ in each track requested, but all selected tracks among all track requests
		// should have the same nature, i.e. do not require reinitialization

		const nulla::track_request &trf = r.tracks.front();
		printf("%s: id: %s, duration: %ld, track-requests: %ld, trf: requested_track_number: %d, tracks: %ld\n",
				__func__, r.id.c_str(), r.duration_msec, r.tracks.size(),
				trf.requested_track_number, trf.media.tracks.size());

		const nulla::track &track = trf.track();
		printf("%s: requested_track_number: %d, track-number: %d, track: %s\n",
				__func__, trf.requested_track_number, track.number, track.str().c_str());

		repr.put("<xmlattr>.mimeType", track.mime_type);
		repr.put("<xmlattr>.codecs", track.codec);
		repr.put("<xmlattr>.bandwidth", track.bandwidth);

		if (track.media_type == GF_ISOM_MEDIA_AUDIO) {
			repr.put("<xmlattr>.audioSamplingRate", track.audio.sample_rate);

			pt::ptree channel;
			channel.put("<xmlattr>.schemeIdUri", "urn:mpeg:dash:23003:3:audio_channel_configuration:2011");
			channel.put("<xmlattr>.value", track.audio.channels);

			repr.add_child("AudioChannelConfiguration", channel);
		} else if (track.media_type == GF_ISOM_MEDIA_VISUAL) {
			u32 fps_num = track.video.fps_num;
			u32 fps_denum = track.video.fps_denum;

			gf_media_get_reduced_frame_rate(&fps_num, &fps_denum);

			repr.put("<xmlattr>.width", track.video.width);
			repr.put("<xmlattr>.height", track.video.height);
			if (fps_denum > 1) {
				repr.put("<xmlattr>.frameRate",
						std::to_string(track.video.fps_num) + "/" +
						std::to_string(track.video.fps_denum));
			} else {
				repr.put("<xmlattr>.frameRate", track.video.fps_num);
			}
			repr.put("<xmlattr>.sar",
					std::to_string(track.video.sar_w) + ":" +
					std::to_string(track.video.sar_h));
		}

		add_segment(repr, r, track);

		aset.add_child("Representation", repr);
	}

	void add_segment(pt::ptree &repr, const nulla::representation &r, const nulla::track &track) {
		pt::ptree seg;

		seg.put("<xmlattr>.timescale", track.media_timescale);
		seg.put("<xmlattr>.duration", track.media_timescale * m_playlist->chunk_duration_sec);
		seg.put("<xmlattr>.initialization", "init/" + r.id);
		seg.put("<xmlattr>.startNumber", 0);
		seg.put("<xmlattr>.media", "play/" + r.id + "/$Number$");

		repr.add_child("SegmentTemplate", seg);
	}

	std::string print_time(long duration_msec) const {
		float sec = (float)duration_msec / 1000.0;
		int h = sec / 3600;
		int m = (sec - h * 3600) / 60;
		float s = sec - h * 3600 - m * 60;
		char tmp[36];
		snprintf(tmp, sizeof(tmp), "PT%dH%dM%.3fS", h, m, s);
		return std::string(tmp);
	}
};

}} // namespace ioremap::nulla

#endif // __NULLA_MPD_GENERATOR_HPP
