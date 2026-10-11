#include "bar_conquest.h"

#include "bar_server_time.h"

namespace headsup
{
    namespace
    {
        std::string Points(const std::optional<ConquestStanding>& standing, uint32_t ConquestStanding::*field)
        {
            return standing ? Grouped((*standing).*field) : "-";
        }
    }

    WidgetContent ConquestWidget(int64_t unixMs, const std::optional<ConquestStanding>& standing)
    {
        WidgetContent w;
        w.shown    = true;
        w.title    = ListOf(Text("Tally " + SpanText(SecondsToTally(unixMs))));
        w.subtitle = ListOf(Text(standing ? Grouped(standing->conquestPoints) + " CP" : "CP -", Tone::Dim));
        w.hasPopup = true;
        w.figures  = true;
        return w;
    }

    PopupContent ConquestPopup(int64_t unixMs, const std::optional<ConquestStanding>& standing)
    {
        PopupContent popup;
        popup.title = ListOf(Text("Conquest"));
        PopupSection rows;
        rows.rows.push_back(Labeled("Conquest points", Text(Points(standing, &ConquestStanding::conquestPoints))));
        rows.rows.push_back(Labeled("Imperial Standing", Text(Points(standing, &ConquestStanding::imperialStanding))));
        rows.rows.push_back(Line(Cell(ListOf(Text("Tally in ", Tone::Dim), Text(SpanText(SecondsToTally(unixMs))),
            Text(" (Monday 00:00 JST)", Tone::Dim)))));
        if (standing)
            rows.rows.push_back(LineOf(std::to_string(standing->tallyVanaDays) +
                                           (standing->tallyVanaDays == 1 ? " Vana'diel day" : " Vana'diel days") +
                                           ", as the server counts them",
                Tone::Dim));
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
