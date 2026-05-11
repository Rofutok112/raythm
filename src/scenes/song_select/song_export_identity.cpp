#include "song_select/song_export_identity.h"

#include "uuid_util.h"

namespace song_select {

std::string regenerated_export_id(const std::string& current_id) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        const std::string generated = generate_uuid();
        if (!generated.empty() && generated != current_id) {
            return generated;
        }
    }

    return generate_uuid();
}

chart_data make_export_chart_copy(const chart_data& chart) {
    chart_data exported = chart;
    exported.meta.chart_id = regenerated_export_id(chart.meta.chart_id);
    exported.meta.song_id.clear();
    return exported;
}

song_meta make_export_song_meta_copy(const song_meta& meta) {
    song_meta exported = meta;
    exported.song_id = regenerated_export_id(meta.song_id);
    return exported;
}

}  // namespace song_select
