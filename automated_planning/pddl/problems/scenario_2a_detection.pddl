( define ( problem scenario_2a_detection )
( :domain search_and_rescue )
( :objects
	d0 - drone
	p0 - person
	h0 h1 a0 a1 a2 a3 a4 a5 a6 a7 elz0 elz1 - location
)
( :init
	( path h0 a0 )
	( path h0 a3 )
	( path h0 a5 )
	( not_searched h0 )
	( available h0 )
	( path h1 a0 )
	( path h1 elz0 )
	( not_searched h1 )
	( available h1 )
	( path a0 h0 )
	( path a0 a1 )
	( path a0 a2 )
	( not_searched a0 )
	( available a0 )
	( path a1 a0 )
	( path a1 elz0 )
	( not_searched a1 )
	( available a1 )
	( path a2 h1 )
	( path a2 a0 )
	( path a2 a3 )
	( not_searched a2 )
	( available a2 )
	( path a3 h0 )
	( path a3 a2 )
	( path a3 a4 )
	( not_searched a3 )
	( available a3 )
	( path a4 a3 )
	( path a4 h1 )
	( not_searched a4 )
	( available a4 )
	( path a5 h0 )
	( path a5 a6 )
	( not_searched a5 )
	( available a5 )
	( path a6 a5 )
	( path a6 a7 )
	( not_searched a6 )
	( available a6 )
	( path a7 a6 )
	( path a7 elz1 )
	( not_searched a7 )
	( available a7 )
	( path elz0 a1 )
	( not_searched elz0 )
	( available elz0 )
	( path elz1 a7 )
	( not_searched elz1 )
	( available elz1 )
	( drone_at d0 h0 )
	( not_moving d0 )
	( can_land h0 )
	( can_land h1 )
	( can_land elz0 )
	( can_land elz1 )
	( can_recharge h0 )
	( can_recharge h1 )
	( can_resupply h0 )
	( can_resupply h1 )
	( not_tracking d0 )
	( not_rescuing d0 )
	( not_marking d0 )
	( not_landed d0 )
	( person_at p0 h0 )
	( not_communicated p0 h0 )
	( not_tracked p0 )
	( = ( distance h0 a0 ) 20 )
	( = ( distance h0 a3 ) 20 )
	( = ( distance h0 a5 ) 20 )
	( = ( search_distance h0 ) 19 )
	( = ( distance h1 a0 ) 44.7214 )
	( = ( distance h1 elz0 ) 60 )
	( = ( search_distance h1 ) 19 )
	( = ( distance a0 h0 ) 20 )
	( = ( distance a0 a1 ) 20 )
	( = ( distance a0 a2 ) 20 )
	( = ( search_distance a0 ) 19 )
	( = ( distance a1 a0 ) 20 )
	( = ( distance a1 elz0 ) 20 )
	( = ( search_distance a1 ) 19 )
	( = ( distance a2 h1 ) 28.2843 )
	( = ( distance a2 a0 ) 20 )
	( = ( distance a2 a3 ) 20 )
	( = ( search_distance a2 ) 19 )
	( = ( distance a3 h0 ) 20 )
	( = ( distance a3 a2 ) 20 )
	( = ( distance a3 a4 ) 20 )
	( = ( search_distance a3 ) 19 )
	( = ( distance a4 a3 ) 20 )
	( = ( distance a4 h1 ) 40 )
	( = ( search_distance a4 ) 19 )
	( = ( distance a5 h0 ) 20 )
	( = ( distance a5 a6 ) 20 )
	( = ( search_distance a5 ) 19 )
	( = ( distance a6 a5 ) 20 )
	( = ( distance a6 a7 ) 20 )
	( = ( search_distance a6 ) 19 )
	( = ( distance a7 a6 ) 20 )
	( = ( distance a7 elz1 ) 20 )
	( = ( search_distance a7 ) 19 )
	( = ( distance elz0 a1 ) 20 )
	( = ( search_distance elz0 ) 19 )
	( = ( distance elz1 a7 ) 20 )
	( = ( search_distance elz1 ) 19 )
	( = ( track_battery_usage d0 ) 0.05896 )
	( = ( move_battery_usage d0 ) 0.06201 )
	( = ( track_velocity d0 ) 0.2 )
	( = ( move_velocity d0 ) 2 )
	( = ( num_markers d0 ) 2 )
	( = ( num_lifevests d0 ) 1 )
	( = ( battery_charge d0 ) 100 )
)
( :goal
	( and
		( communicated p0 h0 )
	)
)
)
