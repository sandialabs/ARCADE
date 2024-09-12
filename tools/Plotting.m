% Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
% Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
% certain rights in this software.

% This program is free software: you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation, either version 3 of the License, or
% (at your option) any later version.

% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.

% You should have received a copy of the GNU General Public License
% along with this program.  If not, see <https://www.gnu.org/licenses/>.

%%Plotting tools for ARCADE
close all

Start_time = 2020.0;
UCA_start_time = 2050.0;
UCA_end_time = 2150.0;
UCA_Names = ["Baseline","UCA #1","UCA #2","UCA #3","UCA Start"];

line_width = 1.0;
fig_num = 1;

%Import Baseline CSV files

baseline_csv.Pub = readtable('Formated\formatted_Baseline_pub_data.csv','VariableNamingRule','preserve');
baseline_csv.Up = readtable('Formated\formatted_Baseline_up_data.csv','VariableNamingRule','preserve');

%fix table names
table_names = baseline_csv.Pub.Properties.VariableNames;
table_names = matlab.lang.makeValidName(table_names);
baseline_csv.Pub.Properties.VariableNames = table_names;

table_names = baseline_csv.Up.Properties.VariableNames;
table_names = matlab.lang.makeValidName(table_names);
baseline_csv.Up.Properties.VariableNames = table_names;

%% TCV UCA Plotting
Title_Name = "Turbine Control Valve UCAs: ";

TCV_Pub_filenames = ["Formated\formatted_TCV_UCA_1_pub_data.csv","Formated\formatted_TCV_UCA_2_pub_data.csv","Formated\formatted_TCV_UCA_5_pub_data.csv"];
TCV_Up_filenames = ["Formated\formatted_TCV_UCA_1_up_data.csv","Formated\formatted_TCV_UCA_2_up_data.csv","Formated\formatted_TCV_UCA_5_up_data.csv"];
UCA_Tags = ["UCA_1","UCA_2","UCA_3"];

Var_plots = ["Max_Fuel_Temp","Av_Fuel_Temp","PV_TurbineValve_Opening","PV_Turbine_Power"];
Var_IDX = ["Pub","Pub","Up","Pub"];
Var_labels = ["Max Fuel Temperature","Average Fuel Temperature","Turbine Valve Opening","Turbine Power"];
Var_units = ["(C)","(C)","(%)","(MW)"];
Var_gain = [1,1,1,1/(10^6)];
Var_offset = [-273.15,-273.15,0,0];

for i=1:length(TCV_Pub_filenames)
    varname = genvarname(UCA_Tags(i));
    %read
    TCV_csv.Pub.(varname) = readtable(TCV_Pub_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = TCV_csv.Pub.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    TCV_csv.Pub.(varname).Properties.VariableNames = table_names;
    
    %read
    TCV_csv.Up.(varname) = readtable(TCV_Up_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = TCV_csv.Up.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    TCV_csv.Up.(varname).Properties.VariableNames = table_names;
end


for i=1:length(Var_plots)
    
    figure(fig_num)

    line_names = UCA_Names;
    plot_title = strcat(Title_Name,Var_labels(i));
    x_label = "Simulation Time (s)";
    y_label = strcat(Var_labels(i)," ",Var_units(i));
    hold on
    grid on
    min_val = 100000;
    max_val = -100000;

    title(plot_title)
    xlabel(x_label)
    ylabel(y_label)

    varname_IDX = genvarname(Var_IDX(i));
    varname_plot = genvarname(Var_plots(i));
    
    %plot baseline
    Baseline_Start_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - Start_time) < 1e-2);
    Baseline_End_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - UCA_end_time) < 1e-2);

    plot(baseline_csv.(varname_IDX).Time(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))-Start_time,baseline_csv.(varname_IDX).(varname_plot)(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
    
    %Plot UCAs
    for n=1:length(UCA_Tags)
     
        varname_UCA = genvarname(UCA_Tags(n));
        UCA_Start_Time_IDX = find(abs(TCV_csv.(varname_IDX).(varname_UCA).Time - Start_time) < 1e-2);
        UCA_End_Time_IDX = find(abs(TCV_csv.(varname_IDX).(varname_UCA).Time - UCA_end_time) < 1e-2);

        if isempty(UCA_End_Time_IDX)
            plot(TCV_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):end-10)-Start_time,TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
            max_check = max(TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
        else
            plot(TCV_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))-Start_time,TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
            max_check = max(TCV_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
        end

        
        if min_val > min_check
            min_val = min_check;
        end
        if max_val < max_check
            max_val = max_check;
        end

    end
    
    pl = line([UCA_start_time-Start_time,UCA_start_time-Start_time],[min_val-5,max_val+5]);
    pl.Color = 'black';
    pl.LineStyle = '--';
    p1.LineWidth = line_width;
    
    ylim([min_val-5,max_val+5])
    legend(UCA_Names)
    fig_num = fig_num + 1;
end


%% CIRC UCA Plotting
Title_Name = "Circulator UCAs: ";

CIRC_Pub_filenames = ["Formated\formatted_CIRC_UCA_1_pub_data.csv","Formated\formatted_CIRC_UCA_2_pub_data.csv","Formated\formatted_CIRC_UCA_3_pub_data.csv"];
CIRC_Up_filenames = ["Formated\formatted_CIRC_UCA_1_up_data.csv","Formated\formatted_CIRC_UCA_2_up_data.csv","Formated\formatted_CIRC_UCA_3_up_data.csv"];
UCA_Tags = ["UCA_1","UCA_2","UCA_3"];

Var_plots = ["Max_Fuel_Temp","Av_Fuel_Temp","PV_BlowerA_Speed","PV_BlowerB_Speed"];
Var_IDX = ["Pub","Pub","Up","Up"];
Var_labels = ["Max Fuel Temperature","Average Fuel Temperature","Circulator A Speed","Circulator B Speed"];
Var_units = ["(C)","(C)","(RPM)","(RPM)"];
Var_gain = [1,1,1,1];
Var_offset = [-273.15,-273.15,0,0];

for i=1:length(TCV_Pub_filenames)
    varname = genvarname(UCA_Tags(i));
    %read
    CIRC_csv.Pub.(varname) = readtable(CIRC_Pub_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = CIRC_csv.Pub.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    CIRC_csv.Pub.(varname).Properties.VariableNames = table_names;
    
    %read
    CIRC_csv.Up.(varname) = readtable(CIRC_Up_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = CIRC_csv.Up.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    CIRC_csv.Up.(varname).Properties.VariableNames = table_names;
end


for i=1:length(Var_plots)
    
    figure(fig_num)

    line_names = UCA_Names;
    plot_title = strcat(Title_Name,Var_labels(i));
    x_label = "Simulation Time (s)";
    y_label = strcat(Var_labels(i)," ",Var_units(i));
    hold on
    grid on
    min_val = 100000;
    max_val = -100000;

    title(plot_title)
    xlabel(x_label)
    ylabel(y_label)

    varname_IDX = genvarname(Var_IDX(i));
    varname_plot = genvarname(Var_plots(i));
    
    %plot baseline
    Baseline_Start_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - Start_time) < 1e-2);
    Baseline_End_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - UCA_end_time) < 1e-2);

    plot(baseline_csv.(varname_IDX).Time(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))-Start_time,baseline_csv.(varname_IDX).(varname_plot)(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
    
    %Plot UCAs
    for n=1:length(UCA_Tags)
     
        varname_UCA = genvarname(UCA_Tags(n));
        UCA_Start_Time_IDX = find(abs(CIRC_csv.(varname_IDX).(varname_UCA).Time - Start_time) < 1e-2);
        UCA_End_Time_IDX = find(abs(CIRC_csv.(varname_IDX).(varname_UCA).Time - UCA_end_time) < 1e-2);

        if isempty(UCA_End_Time_IDX)
            plot(CIRC_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):end-10)-Start_time,CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
            max_check = max(CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
        else
            plot(CIRC_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))-Start_time,CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
            max_check = max(CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
        end

        
        if min_val > min_check
            min_val = min_check;
        end
        if max_val < max_check
            max_val = max_check;
        end

    end
    
    pl = line([UCA_start_time-Start_time,UCA_start_time-Start_time],[min_val-5,max_val+5]);
    pl.Color = 'black';
    pl.LineStyle = '--';
    p1.LineWidth = line_width;
    
    ylim([min_val-5,max_val+5])
    legend(UCA_Names)
    fig_num = fig_num + 1;
end



%% TCV CIRC UCA Plotting
Title_Name = "Combine Circulator & TCV UCAs: ";

TCV_CIRC_Pub_filenames = ["Formated\formatted_UCA_CIRC_TCV_1_pub_data.csv","Formated\formatted_UCA_CIRC_TCV_2_pub_data.csv","Formated\formatted_UCA_CIRC_TCV_3_pub_data.csv"];
TCV_CIRC_Up_filenames = ["Formated\formatted_UCA_CIRC_TCV_1_up_data.csv","Formated\formatted_UCA_CIRC_TCV_2_up_data.csv","Formated\formatted_UCA_CIRC_TCV_3_up_data.csv"];
UCA_Tags = ["UCA_1","UCA_2","UCA_3"];

Var_plots = ["Max_Fuel_Temp","Av_Fuel_Temp","PV_BlowerA_Speed","PV_BlowerB_Speed","PV_TurbineValve_Opening","PV_Turbine_Power"];
Var_IDX = ["Pub","Pub","Up","Up","Up","Pub"];
Var_labels = ["Max Fuel Temperature","Average Fuel Temperature","Circulator A Speed","Circulator B Speed","Turbine Valve Opening","Turbine Power"];
Var_units = ["(C)","(C)","(RPM)","(RPM)","(%)","(MW)"];
Var_gain = [1,1,1,1,1,1/(10^6)];
Var_offset = [-273.15,-273.15,0,0,0,0];

for i=1:length(TCV_Pub_filenames)
    varname = genvarname(UCA_Tags(i));
    %read
    TCV_CIRC_csv.Pub.(varname) = readtable(TCV_CIRC_Pub_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = TCV_CIRC_csv.Pub.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    TCV_CIRC_csv.Pub.(varname).Properties.VariableNames = table_names;
    
    %read
    TCV_CIRC_csv.Up.(varname) = readtable(TCV_CIRC_Up_filenames(i),'VariableNamingRule','preserve');
    %fix
    table_names = TCV_CIRC_csv.Up.(varname).Properties.VariableNames;
    table_names = matlab.lang.makeValidName(table_names);
    TCV_CIRC_csv.Up.(varname).Properties.VariableNames = table_names;
end


for i=1:length(Var_plots)
    
    f = figure(fig_num);
    line_names = UCA_Names;
    plot_title = strcat(Title_Name,Var_labels(i));
    x_label = "Simulation Time (s)";
    y_label = strcat(Var_labels(i)," ",Var_units(i));
    hold on
    grid on
    min_val = 100000;
    max_val = -100000;

    title(plot_title)
    xlabel(x_label)
    ylabel(y_label)

    varname_IDX = genvarname(Var_IDX(i));
    varname_plot = genvarname(Var_plots(i));
    
    %plot baseline
    Baseline_Start_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - Start_time) < 1e-2);
    Baseline_End_Time_IDX = find(abs(baseline_csv.(varname_IDX).Time - UCA_end_time) < 1e-2);

    plot(baseline_csv.(varname_IDX).Time(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))-Start_time,baseline_csv.(varname_IDX).(varname_plot)(Baseline_Start_Time_IDX(1):Baseline_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
    
    %Plot UCAs
    for n=1:length(UCA_Tags)
     
        varname_UCA = genvarname(UCA_Tags(n));
        UCA_Start_Time_IDX = find(abs(TCV_CIRC_csv.(varname_IDX).(varname_UCA).Time - Start_time) < 1e-2);
        UCA_End_Time_IDX = find(abs(TCV_CIRC_csv.(varname_IDX).(varname_UCA).Time - UCA_end_time) < 1e-2);

        if isempty(UCA_End_Time_IDX)
            plot(TCV_CIRC_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):end-10)-Start_time,TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
            max_check = max(TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):end-10)*Var_gain(i)+Var_offset(i));
        else
            plot(TCV_CIRC_csv.(varname_IDX).(varname_UCA).Time(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))-Start_time,TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i),'LineWidth',line_width)
            min_check = min(TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
            max_check = max(TCV_CIRC_csv.(varname_IDX).(varname_UCA).(varname_plot)(UCA_Start_Time_IDX(1):UCA_End_Time_IDX(1))*Var_gain(i)+Var_offset(i));
        end

        
        if min_val > min_check
            min_val = min_check;
        end
        if max_val < max_check
            max_val = max_check;
        end

    end
    
    pl = line([UCA_start_time-Start_time,UCA_start_time-Start_time],[min_val-5,max_val+5]);
    pl.Color = 'black';
    pl.LineStyle = '--';
    p1.LineWidth = line_width;
    
    ylim([min_val-5,max_val+5])
    legend(UCA_Names)
    %fontsize(scale=1.5)
    %ax = gca;
    %set(ax,'Position',[100 100 500 500])
    %set(ax,'Units','inches','OuterPosition', [0 0 10 5])
    %ax.LineWidth = 2.5;
    %exportgraphics(ax,strcat(matlab.lang.makeValidName(plot_title),".tif"))
    fig_num = fig_num + 1;
end