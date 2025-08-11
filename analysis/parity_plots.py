import numpy as np
import util
import matplotlib as mpl
mpl.rcParams['font.size'] = 15  # Change 12 to your desired font size

import matplotlib.pyplot as plt

from labels import labels, texts, info

SHOW_ZOOMED_AREA = True

plt.rcParams['axes.prop_cycle'] = plt.cycler(color=util.COLORS)
mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42

TICK_FREQUENCY = 1
MARK_EVERY = 2

def get_max_owd_data_transformation_fn(data_point):
    return data_point[0]

def get_diff_between_max_min_owd_data_transformation_fn(data_point):
    return data_point[0]-data_point[1]

def get_mean_holding_duration_data_transformation_fn(data_point):
    if (len(data_point) < 3):
        return 0
    return data_point[2]

def get_max_holding_duration_data_transformation_fn(data_point):
    if (len(data_point) < 4):
        return 0
    return data_point[3]

def get_label_for_data_transformation_fn(data_transformation):
    if data_transformation == get_max_owd_data_transformation_fn:
        return "Overall Multicast Latency (microseconds)"
    if data_transformation == get_diff_between_max_min_owd_data_transformation_fn:
        return "Delivery Window Size (microseconds)"
    if data_transformation == get_mean_holding_duration_data_transformation_fn:
        return "Mean Holding Duration (microseconds)"
    if data_transformation == get_max_holding_duration_data_transformation_fn:
        return "Max Holding Duration (microseconds)"
    print("No Label Defined")
    return "No Label Defined"

def plot_x_y(x, y, owds, label, index, linestyle, plot_ci, graph=util.Graph.CDF):
    # MARK_EVERY = int((len(x)+1)/50)
    # print(MARK_EVERY, int((len(x)+1)/50))
    if (linestyle == ""):
        if ("Dyn" in label and "Fix" not in label):
            linestyle=util.LINESTYLES[0]
        elif ("Dyn" not in label and "Fix" in label):
            linestyle=util.LINESTYLES[1]
        else:
            linestyle=util.LINESTYLES[index%len(util.LINESTYLES)]

    line, = plt.plot(
        x, y, label=label, color=util.COLORS[index%len(util.COLORS)],
        linestyle=linestyle, linewidth=3) # marker=util.MARKERS[index%len(util.MARKERS)])# markevery=MARK_EVERY, )

    line_color = line.get_color()
    if plot_ci:
        ci_lower, ci_upper = util.bootstrap_ci.get_bootstrap_ci(owds, x, bootstrap_samples=100)
        plt.fill_between(x, ci_lower, ci_upper, color=line_color, alpha=1)

    # xticks_positions = x
    # xticks_labels = [str(x) for x in xticks_positions]
    # for i, ele in enumerate(xticks_labels):
    #     if i % TICK_FREQUENCY != 0:
    #         xticks_labels[i] = ''
    # xticks_labels[0] = str(xticks_positions[0])
    # xticks_labels[-1] = str(xticks_positions[-1])
    # plt.xticks(xticks_positions, xticks_labels)
    # ticks = np.arange(min(x), max(x)+1, TICK_FREQUENCY)
    # ticks = list(ticks) # + [max(x)]
    # plt.xticks(ticks)

    return line

def plot_even_odd(
        experimentt_label, max_min_owd_per_msg_id,
        texts, index, plot_ci, do_even_odd_split,
        graph=util.Graph.CDF, PERCENTILES_TO_SHOW=[],
        data_transformation=get_max_owd_data_transformation_fn):
    global TICK_FREQUENCY
    owds_even = []
    owds_odd = []
    msg_ids_even = []
    msg_ids_odd = []
    owds = []
    msg_ids = []

    for msg_id in max_min_owd_per_msg_id:
        owds += [data_transformation(max_min_owd_per_msg_id[msg_id])]
        msg_ids += [int(msg_id)]

        if int(msg_id)%2:
            owds_odd += [data_transformation(max_min_owd_per_msg_id[msg_id])]
            msg_ids_odd += [int(msg_id)]
        else:
            owds_even += [data_transformation(max_min_owd_per_msg_id[msg_id])]
            msg_ids_even += [int(msg_id)]

    if do_even_odd_split:
        if graph == util.Graph.CDF:
            x = PERCENTILES_TO_SHOW
            y_odd = []
            y_even = []
            y_odd = util.np.percentile(owds_odd, x) # Odd is hedged data
            y_even = util.np.percentile(owds_even, x)

            # plt.yscale('log')  # Set the y-scale to logarithmic
            plot_x_y(y_odd, x, owds_odd, f"Hedging, {texts[experimentt_label]}", index, ":", plot_ci)
            plot_x_y(y_even, x, owds_even, f"No Hedging counterpart of {texts[experimentt_label]}", index, "solid", plot_ci)
            # plt.ylim([0, 500])
            plt.title(f"{get_label_for_data_transformation_fn(data_transformation)} Across All Receivers")
            plt.ylabel("Percentiles")
            plt.xlabel("OWD (microseconds)")

        if graph == util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE:
            x = PERCENTILES_TO_SHOW
            y_odd = []
            y_even = []
            y_odd = util.np.percentile(owds_odd, x) # Odd is hedged data
            y_even = util.np.percentile(owds_even, x)

            percentage_improvements = []
            for i in range(0, len(y_even)):
                a = 100*(y_even[i]-y_odd[i])/abs(y_even[i])
                percentage_improvements += [a]
            plot_x_y(x, percentage_improvements, [], texts[experimentt_label], index,
                    linestyle=util.LINESTYLES[index%len(util.LINESTYLES)], plot_ci=False, graph=graph)
            plt.title(f"Percentage Improvement In {get_label_for_data_transformation_fn(data_transformation)} At Different Percentiles")
            plt.xlabel("Percentiles")
            plt.ylabel("Percentage Improvement (%)")

        if graph == util.Graph.TIME_SERIES:
            TICK_FREQUENCY = 500
            step = 1
            aggregation_percentile = 99
            aggregation_step = 500
            if experimentt_label in info:
                aggregation_step = info[experimentt_label]["msg_rate"]
                TICK_FREQUENCY = aggregation_step

            plt.ylim([0, 800])
            msg_ids_even, owds_even = util.sort_one_reorder_the_other(msg_ids_even, owds_even)
            msg_ids_odd, owds_odd = util.sort_one_reorder_the_other(msg_ids_odd, owds_odd)

            owds_even, ids_even = util.aggregate_time_series(owds_even, aggregation_percentile, aggregation_step)
            owds_odd, ids_odd = util.aggregate_time_series(owds_odd, aggregation_percentile, aggregation_step)

            plot_x_y(ids_odd[::step], owds_odd[::step], [], f"Hedging, {texts[experimentt_label]}", index, ":", False)
            plot_x_y(ids_even[::step], owds_even[::step], [], f"No Hedging, {texts[experimentt_label]}", index+1, "-", False)
            plt.title(f"Time SSSeries Representation of {get_label_for_data_transformation_fn(data_transformation)}")
            plt.xlabel("Time (relative values)")
            plt.ylabel(f"OWD (microseconds)\n{aggregation_percentile}th percentile over tumbling window of {aggregation_step} messages")
    else:
        x = []
        y = []
        x = PERCENTILES_TO_SHOW
        if graph == util.Graph.CDF:
            y = util.np.percentile(owds, x)
            # plt.yscale('log')  # Set the y-scale to logarithmic
            plot_x_y(y, x, owds, texts[experimentt_label], index, "", plot_ci)
            # plt.ylim([50, 100])

            # if y[50] == 0:
            #     plt.xlim([-200, max(y)+100])
            # else:
            #     plt.xlim([0, max(y)+100])
            
            # if y[50] == 0:
            #     plt.xlim([-200, 1400])
            # else:
            #     plt.xlim([0, 1400])
            # print("Median0 Latency: ", np.percentile(owds, 30.0))
            print("Median Latency: ", np.percentile(owds, 50.0))
            print("75th Latency: ", np.percentile(owds, 75.0))
            print("90th Latency: ", np.percentile(owds, 90.0))
            print("95th Latency: ", np.percentile(owds, 95.0))
            print("99thh Latency: ", np.percentile(owds, 99.0))
            tmp = [x for x in range(99, 30, -1)]
            for i in tmp:
                if round(np.percentile(owds, i)) == 0:
                    print("Perfect Fariness In Data: ", i)
                    break

            # plt.title(f"{get_label_for_data_transformation_fn(data_transformation)}")
            # plt.title(f"Holding Duration Analysis")
            # plt.title(f"Overall Multicast Latency (OML)")
            # plt.title(f"Delivery Window Analysis")
            # plt.xlabel("Delivery Window Size(microseconds)")
            # plt.xlabel("Mean Message Holding Duration (microseconds)")
            # plt.xlabel("Overall Multicast Latency (microseconds)")
            plt.xlabel(f"{get_label_for_data_transformation_fn(data_transformation)}")
            plt.ylabel("CDF")

        if graph == util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE:
            print("TODO")

        if graph == util.Graph.TIME_SERIES or graph == util.Graph.TIME_SERIES_VARIANCE:
            # TICK_FREQUENCY = 1000
            # plt.ylim([0, 1000])
            aggregation_percentile = 90
            aggregation_step = 5000

            if experimentt_label in info:
                aggregation_step = int(info[experimentt_label]["msg_rate"])
                # TICK_FREQUENCY = aggregation_step

            msg_ids, owds = util.sort_one_reorder_the_other(msg_ids, owds)
            owds, msg_ids = util.aggregate_time_series(owds, aggregation_percentile, aggregation_step, graph == util.Graph.TIME_SERIES_VARIANCE)
            print(owds[0], owds[-1])
            step = 1
            plot_x_y(msg_ids[::step], owds[::step], [], f"{texts[experimentt_label]}", index, "", False)
            extra = ""
            if graph == util.Graph.TIME_SERIES_VARIANCE:
                extra = "std. dev. of "
            # plt.title(f"Time Series Representation of {extra}{get_label_for_data_transformation_fn(data_transformation)}")
            # plt.title(f"Temporal Variance Analysis")
            plt.xlabel("Time (seconds)")
            plt.yticks(rotation=60)

            plt.ylabel(f"OML (us) over a tumbling window of 1s")
            if graph == util.Graph.TIME_SERIES_VARIANCE:
                plt.ylabel(f"Std. Dev. of OWD (us)")

def plot_all(
        labels, texts, do_even_odd_split=False,
        graph=util.Graph.CDF, save_each_file=False,
        PERCENTILES_TO_SHOW=[], data_transformation=get_max_owd_data_transformation_fn):
    if save_each_file == False:
        plt.figure()

    for ind, l in enumerate(labels):
        if save_each_file:
            plt.figure()

        with open(f"{util.DATA_DIR}{l}.json") as f:
            data = util.json.loads(f.read())

        oe_split = do_even_odd_split
        if l in info and "parity" in info[l]:
            oe_split = info[l]["parity"]

        plot_even_odd(
            l, data, texts, ind, plot_ci=False,
            do_even_odd_split=oe_split,
            graph=graph, PERCENTILES_TO_SHOW=PERCENTILES_TO_SHOW,
            data_transformation=data_transformation)

        plt.tight_layout()
        plt.legend(fontsize='large', prop={'size': 22}) #fontsize='large'
        # plt.legend() #fontsize='large'

        if save_each_file:
            name = f"{l}-{graph}-{PERCENTILES_TO_SHOW[0]}{ind}{get_label_for_data_transformation_fn(data_transformation)}"
            plt.savefig(f"{util.FIGS_DIR}{name}.pdf")

    if save_each_file == False:
        name = f"{l}-{graph}{PERCENTILES_TO_SHOW[0]}{get_label_for_data_transformation_fn(data_transformation)}"
        plt.savefig(f"{util.FIGS_DIR}{name}.pdf")
    return
